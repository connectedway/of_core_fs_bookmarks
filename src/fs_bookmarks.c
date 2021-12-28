/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#undef DEBUG_FUNCTION_CALLS

#include "ofc/types.h"
#include "ofc/handle.h"
#include "ofc/libc.h"
#include "ofc/path.h"
#include "ofc/framework.h"
#include "ofc/thread.h"

#include "ofc/heap.h"
#include "ofc/file.h"
#include "ofc/fs.h"
#include "ofc/config.h"

#include "ofc/fs_bookmarks.h"
/**
 * \defgroup BlueFSBookmarks Bookmark Abstraction
 * \ingroup BlueFS
 *
 */

/** \{ */

#define BOOKMARK_ELEMENT_LEN 16
typedef struct
{
  OFC_BOOL hidden ;
  OFC_TCHAR bookmark[BOOKMARK_ELEMENT_LEN] ;
} BOOKMARK_ELEMENT ;

/**
 * Internal Structure for Bookmark FS Handler
 */
typedef struct 
{
  /**
   * The Client Handle from the lower layer
   */
  BLUE_HANDLE hFile ;
  /*
   * Other data used for an open directory for listing contents
   */
  OFC_UINT16 search_count ;
  OFC_UINT16 search_index ;
  BOOKMARK_ELEMENT *bookmark_elements ;
} BOOKMARK_FILE ;

static OFC_VOID PopulateResults (BOOKMARK_FILE *bookmark_file,
				  BLUE_FRAMEWORK_MAPS *maps) 
{
  OFC_INT i ;
  OFC_INT old_count ;
  OFC_INT search_count ;
  OFC_TCHAR *element ;

  DBG_ENTRY() ;
  old_count = bookmark_file->search_count ;
  search_count = maps->numMaps ;

  /*
   * Realloc to hold all.  We'll shrink it after we filter
   */
  bookmark_file->bookmark_elements = 
    BlueHeapRealloc (bookmark_file->bookmark_elements,
		     (old_count+search_count) * sizeof (BOOKMARK_ELEMENT)) ;

  for (i = 0 ; i < search_count ; i++)
    {
      element = maps->map[i].prefix ;
      
      bookmark_file->bookmark_elements[bookmark_file->search_count].hidden =
	OFC_FALSE ;
      if (maps->map[i].type != BLUE_FS_FILE)
	bookmark_file->bookmark_elements[bookmark_file->search_count].hidden =
	  OFC_TRUE ;

      BlueCtstrncpy 
	(bookmark_file->
	 bookmark_elements[bookmark_file->search_count++].bookmark,
	 element, BOOKMARK_ELEMENT_LEN) ;
    }
  DBG_EXIT() ;
}

static OFC_INT FindSpot (BOOKMARK_ELEMENT *new_elements, OFC_INT new_count,
			  BOOKMARK_ELEMENT *old_element)
{
  OFC_BOOL found ;
  OFC_INT i ;

  found = OFC_FALSE ;
  i = 0 ;

  while (i < new_count && BlueCtstrncmp ((OFC_TCHAR *) old_element->bookmark, 
					new_elements[i].bookmark, 
					BOOKMARK_ELEMENT_LEN) > 0)
    i++ ;

  return (i) ;
}

static OFC_VOID PushElements (BOOKMARK_ELEMENT *new_elements, 
			       OFC_INT new_index, 
			       OFC_INT new_count)
{
  OFC_INT count ;

  for (count = new_count ; count > new_index ; count--)
    {
      new_elements[count].hidden = new_elements[count-1].hidden ;
      BlueCtstrncpy (new_elements[count].bookmark,
		     new_elements[count-1].bookmark,
		     BOOKMARK_ELEMENT_LEN) ;
    }
}

static OFC_VOID SortResults (BOOKMARK_FILE *bookmark_file)
{
  OFC_INT old_index ;
  OFC_INT new_count ;
  OFC_INT new_index ;
  BOOKMARK_ELEMENT *new_elements ;

  /*
   * First, create a new bookmark elements array
   */
  DBG_ENTRY() ;
  new_elements = BlueHeapMalloc (bookmark_file->search_count * 
				 sizeof (BOOKMARK_ELEMENT)) ;

  new_count = 0 ;
  for (old_index = 0 ; old_index < bookmark_file->search_count ; old_index++)
    {
      /*
       * Find the spot for the old_element in the new array
       * New index will point before the spot.  
       */
      new_index = FindSpot (new_elements, new_count,
			    &bookmark_file->bookmark_elements[old_index]) ;
      /*
       * There are a couple of scenarios now.
       * new_index == new_count.  Element appends to the end
       * old_element[old_index] == new_element[new_index] Skip element
       * new_index < new_count 
       */
      if (new_index == new_count ||
	  BlueCtstrncmp (bookmark_file->bookmark_elements[old_index].bookmark,
			 new_elements[new_index].bookmark,
			 BOOKMARK_ELEMENT_LEN) != 0)
	{
	  /*
	   * We want to add the element.
	   *
	   * First make space for the element.  Can't do a memcpy because
	   * this is an inplace copy
	   *
	   * psrc is the current end of the element array
	   * pdst is the new end of the element array
	   * count is the number of characters to push back
	   */
	  PushElements (new_elements, new_index, new_count) ;
	  /*
	   * Now insert the old element
	   */
	  BlueCtstrncpy (new_elements[new_index].bookmark,
			 bookmark_file->bookmark_elements[old_index].bookmark,
			 BOOKMARK_ELEMENT_LEN) ;
	  new_elements[new_index].hidden =
	    bookmark_file->bookmark_elements[old_index].hidden ;
	  /*
	   * And increment the new count
	   */
	  new_count++ ;
	}
    }
  BlueHeapFree (bookmark_file->bookmark_elements) ;
  bookmark_file->bookmark_elements = new_elements ;
  bookmark_file->search_count = new_count ;
  DBG_EXIT() ;
}

static OFC_VOID ReturnNext (OFC_LPWIN32_FIND_DATAW lpFindFileData,
			     BOOKMARK_FILE *bookmark_file, OFC_BOOL *more)
{
  OFC_SIZET len ;
  BOOKMARK_ELEMENT *bookmark_element ;

  DBG_ENTRY() ;
  bookmark_element = 
    &bookmark_file->bookmark_elements[bookmark_file->search_index] ;

  BlueCtstrncpy (lpFindFileData->cFileName, bookmark_element->bookmark,
		 BOOKMARK_ELEMENT_LEN-3) ;
  len = BlueCtstrnlen (lpFindFileData->cFileName, BOOKMARK_ELEMENT_LEN-3) ;
  lpFindFileData->cFileName[len] = TCHAR(':') ;
  lpFindFileData->cFileName[len+1] = TCHAR('/') ;
  lpFindFileData->cFileName[len+2] = TCHAR_EOS ;

  lpFindFileData->dwFileAttributes = OFC_FILE_ATTRIBUTE_DIRECTORY |
    OFC_FILE_ATTRIBUTE_BOOKMARK ;
  if (bookmark_element->hidden)
    lpFindFileData->dwFileAttributes |= OFC_FILE_ATTRIBUTE_HIDDEN ;
  bookmark_file->search_index++ ;
  if (bookmark_file->search_index < bookmark_file->search_count)
    *more = OFC_TRUE ;
  DBG_EXIT() ;
}

static BLUE_HANDLE
BlueFSBookmarksFindFirst (OFC_LPCTSTR lpFileName,
			  OFC_LPWIN32_FIND_DATAW lpFindFileData,
			  OFC_BOOL *more)
{
  BLUE_HANDLE hFile ;
  BOOKMARK_FILE *bookmark_file ;
  BLUE_FRAMEWORK_MAPS *bookmark_maps ;
				 
  hFile = BLUE_INVALID_HANDLE_VALUE ;
  *more = OFC_FALSE ;

  bookmark_file = BlueHeapMalloc (sizeof (BOOKMARK_FILE)) ;
  if (bookmark_file == OFC_NULL)
    {
      BlueThreadSetVariable (OfcLastError, 
			     (OFC_DWORD_PTR) OFC_ERROR_NOT_ENOUGH_MEMORY) ;
    }
  else
    {
      bookmark_file->search_count = 0 ;
      bookmark_file->bookmark_elements = OFC_NULL ;
      bookmark_file->search_index = 0 ;

      bookmark_maps = BlueFrameworkGetMaps() ;
      PopulateResults (bookmark_file, bookmark_maps) ;
      BlueHeapFree (bookmark_maps) ;

      SortResults (bookmark_file) ;
      
      if (bookmark_file->search_index < bookmark_file->search_count)
	{
	  ReturnNext (lpFindFileData, bookmark_file, more) ;
	  hFile = BlueHandleCreate 
	    (BLUE_HANDLE_FSBOOKMARK_FILE, bookmark_file) ;
	}
      else
	{
	  BlueThreadSetVariable (OfcLastError, 
				 (OFC_DWORD_PTR) OFC_ERROR_NO_MORE_FILES) ;
	}

      if (hFile == BLUE_INVALID_HANDLE_VALUE)
	{
	  BlueHeapFree (bookmark_file->bookmark_elements) ;
	  BlueHeapFree (bookmark_file) ;
	}
    }

  return (hFile) ;
}

static OFC_BOOL
BlueFSBookmarksFindNext (BLUE_HANDLE hFindFile,
			 OFC_LPWIN32_FIND_DATAW lpFindFileData,
			 OFC_BOOL *more)
{
  OFC_BOOL ret ;
  BOOKMARK_FILE *bookmark_file ;

  ret = OFC_FALSE ;

  bookmark_file = BlueHandleLock (hFindFile) ;
  if (bookmark_file != OFC_NULL)
    {
      *more = OFC_FALSE ;
      if (bookmark_file->search_index < bookmark_file->search_count)
	{
	  ReturnNext (lpFindFileData, bookmark_file, more) ;
	  ret = OFC_TRUE ;
	}
      else
	{
	  BlueThreadSetVariable (OfcLastError, 
				 (OFC_DWORD_PTR) OFC_ERROR_NO_MORE_FILES) ;
	}

      BlueHandleUnlock (hFindFile) ;
    }

  return (ret) ;
}

static OFC_BOOL
BlueFSBookmarksFindClose (BLUE_HANDLE hFindFile) 
{
  OFC_BOOL ret ;
  BOOKMARK_FILE *bookmark_file ;

  ret = OFC_FALSE ;

  bookmark_file = BlueHandleLock (hFindFile) ;
  if (bookmark_file != OFC_NULL)
    {
      ret = OFC_TRUE ;
      BlueHeapFree(bookmark_file->bookmark_elements) ;
      BlueHeapFree (bookmark_file) ;
      BlueHandleDestroy (hFindFile) ;
      BlueHandleUnlock (hFindFile) ;
    }
  return (ret) ;
}

static OFC_BOOL 
BlueFSBookmarksGetAttributesEx (OFC_LPCTSTR lpFileName,
				OFC_GET_FILEEX_INFO_LEVELS fInfoLevelId,
				OFC_LPVOID lpFileInformation) 
{
  OFC_BOOL ret ;
  OFC_WIN32_FILE_ATTRIBUTE_DATA *fFileInfo ;

  ret = OFC_TRUE ;
  fFileInfo = (OFC_WIN32_FILE_ATTRIBUTE_DATA *) lpFileInformation ;
  fFileInfo->dwFileAttributes = OFC_FILE_ATTRIBUTE_DIRECTORY ;
  fFileInfo->ftCreateTime.dwLowDateTime = 0 ;
  fFileInfo->ftCreateTime.dwHighDateTime = 0 ;
  fFileInfo->ftLastAccessTime.dwLowDateTime = 0 ;
  fFileInfo->ftLastAccessTime.dwHighDateTime = 0 ;
  fFileInfo->ftLastWriteTime.dwLowDateTime = 0 ;
  fFileInfo->ftLastWriteTime.dwHighDateTime = 0 ;
  fFileInfo->nFileSizeHigh = 0 ;
  fFileInfo->nFileSizeLow = 0 ;
  return (ret) ;
}

static BLUE_FILE_FSINFO BlueFSBookmarksInfo =
  {
    OFC_NULL,
    OFC_NULL,
    &BlueFSBookmarksFindFirst,
    &BlueFSBookmarksFindNext,
    &BlueFSBookmarksFindClose,
    OFC_NULL,
    &BlueFSBookmarksGetAttributesEx,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL,
    OFC_NULL
  } ;

OFC_VOID 
BlueFSBookmarksStartup (OFC_VOID)
{
  BLUE_PATH *path ;

  BlueFSRegister (BLUE_FS_BOOKMARKS, &BlueFSBookmarksInfo) ;
  /*
   * Create a path for the IPC service.  This will add the pattern:
   * BROWSE:/ to the redirector.  When someone does a find first with the
   * browse path, it will come in here
   */
  path = BluePathCreateW (TSTR("")) ;
  if (path == OFC_NULL)
    BlueCprintf ("Couldn't Create Bookmarks Path\n") ;
  else
    {
      BluePathAddMapW (TSTR("Bookmarks"), TSTR("Bookmarks"), path, 
		       BLUE_FS_BOOKMARKS, OFC_TRUE) ;
    }
}

OFC_VOID 
BlueFSBookmarksShutdown (OFC_VOID)
{
  BluePathDeleteMapW (TSTR("Bookmarks"));
}

