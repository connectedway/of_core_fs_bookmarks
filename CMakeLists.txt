cmake_minimum_required(VERSION 3.21.1)
project(of_core_fs_bookmarks VERSION 1.0.1 DESCRIPTION "OpenFiles File Handler for Bookmarks")

include_directories(
	${of_core_BINARY_DIR}
	${of_core_SOURCE_DIR}/include
	include
	)

set(SRCS
    src/fs_bookmarks.c
    )

add_library(of_core_fs_bookmarks STATIC ${SRCS})

set_target_properties(of_core_fs_bookmarks PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(of_core_fs_bookmarks PROPERTIES SOVERSION 1)

