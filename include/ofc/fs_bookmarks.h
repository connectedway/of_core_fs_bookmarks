/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#if !defined(__OFC_FSBOOKMARKS_H__)
#define __OFC_FSBOOKMARKS_H__

#include "ofc/handle.h"
#include "ofc/types.h"
#include "ofc/file.h"

/**
 * \defgroup BlueFSBookmarks Bookmark Abstraction
 * \ingroup BlueFS
 *
 */

/** \{ */

#if defined(__cplusplus)
extern "C"
{
#endif
  OFC_VOID BlueFSBookmarksStartup (OFC_VOID) ;
  OFC_VOID BlueFSBookmarksShutdown (OFC_VOID);
#if defined(__cplusplus)
}
#endif

#endif
/** \} */
