cmake_minimum_required(VERSION 3.20.0)
project(of_core_fs_bookmarks VERSION 1.0.1 DESCRIPTION "OpenFiles File Handler for Bookmarks")

include_directories(
        ${of_core_SOURCE_DIR}/include
        include
)

set(SRCS
        src/fs_bookmarks.c
        )

add_library(of_core_fs_bookmarks OBJECT ${SRCS})
set_property(TARGET of_core_fs_bookmarks PROPERTY POSITION_INDEPENDENT_CODE ON)

