# cmake/FindLexbor.cmake
#
# Find the lexbor HTML5 parser library.
#
# This module defines the following variables:
#   Lexbor_FOUND - True if lexbor is found.
#   Lexbor_INCLUDE_DIRS - The lexbor include directories.
#   Lexbor_LIBRARIES - The lexbor libraries.
#   Lexbor_VERSION - The version of lexbor found.
#
# This module defines the following imported targets:
#   Lexbor::Lexbor - The lexbor library.
#
# Hints:
#   TURNDOWN_PREFER_STATIC - If set, prefer static libraries

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_Lexbor QUIET lexbor liblexbor)
endif()

find_path(Lexbor_INCLUDE_DIR
  NAMES lexbor/html/parser.h
  HINTS
    ${PC_Lexbor_INCLUDEDIR}
    ${PC_Lexbor_INCLUDE_DIRS}
)

# Save and modify library suffixes to prefer static if requested
if(TURNDOWN_PREFER_STATIC)
  set(_Lexbor_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  if(WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a)
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  endif()
endif()

find_library(Lexbor_LIBRARY
  NAMES lexbor liblexbor lexbor_static liblexbor_static
  HINTS
    ${PC_Lexbor_LIBDIR}
    ${PC_Lexbor_LIBRARY_DIRS}
)

# Restore original suffixes
if(TURNDOWN_PREFER_STATIC)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_Lexbor_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

if(NOT Lexbor_VERSION AND PC_Lexbor_VERSION)
  set(Lexbor_VERSION "${PC_Lexbor_VERSION}")
endif()

if(Lexbor_INCLUDE_DIR AND EXISTS "${Lexbor_INCLUDE_DIR}/lexbor/core/base.h")
  file(STRINGS "${Lexbor_INCLUDE_DIR}/lexbor/core/base.h" LEXBOR_VERSION_MAJOR_LINE REGEX "^#define[ \t]+LXB_VERSION_MAJOR[ \t]+[0-9]+")
  file(STRINGS "${Lexbor_INCLUDE_DIR}/lexbor/core/base.h" LEXBOR_VERSION_MINOR_LINE REGEX "^#define[ \t]+LXB_VERSION_MINOR[ \t]+[0-9]+")
  file(STRINGS "${Lexbor_INCLUDE_DIR}/lexbor/core/base.h" LEXBOR_VERSION_PATCH_LINE REGEX "^#define[ \t]+LXB_VERSION_PATCH[ \t]+[0-9]+")
  if(LEXBOR_VERSION_MAJOR_LINE AND LEXBOR_VERSION_MINOR_LINE AND LEXBOR_VERSION_PATCH_LINE)
    string(REGEX REPLACE "^#define[ \t]+LXB_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" LEXBOR_VERSION_MAJOR "${LEXBOR_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+LXB_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" LEXBOR_VERSION_MINOR "${LEXBOR_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+LXB_VERSION_PATCH[ \t]+([0-9]+).*" "\\1" LEXBOR_VERSION_PATCH "${LEXBOR_VERSION_PATCH_LINE}")
    set(Lexbor_VERSION "${LEXBOR_VERSION_MAJOR}.${LEXBOR_VERSION_MINOR}.${LEXBOR_VERSION_PATCH}")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lexbor
  REQUIRED_VARS Lexbor_LIBRARY Lexbor_INCLUDE_DIR
  VERSION_VAR Lexbor_VERSION
)

if(Lexbor_FOUND)
  set(Lexbor_INCLUDE_DIRS ${Lexbor_INCLUDE_DIR})
  set(Lexbor_LIBRARIES ${Lexbor_LIBRARY})

  if(NOT TARGET Lexbor::Lexbor)
    add_library(Lexbor::Lexbor UNKNOWN IMPORTED)
    set_target_properties(Lexbor::Lexbor PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Lexbor_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${Lexbor_LIBRARY}"
    )
  endif()

  # When linking against a static Lexbor library on Windows, consumers must define
  # LEXBOR_STATIC so the headers don't mark symbols as __declspec(dllimport).
  # Otherwise, you'll see unresolved externals like __imp_lxb_* at link time.
  if(TURNDOWN_PREFER_STATIC
      OR Lexbor_LIBRARY MATCHES "lexbor_static"
      OR Lexbor_LIBRARY MATCHES "liblexbor_static"
      OR Lexbor_LIBRARY MATCHES "\\\\.a$")
    set_property(TARGET Lexbor::Lexbor APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS LEXBOR_STATIC)
  endif()
endif()

mark_as_advanced(Lexbor_INCLUDE_DIR Lexbor_LIBRARY)

