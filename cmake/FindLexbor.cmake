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

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_Lexbor QUIET lexbor)

find_path(Lexbor_INCLUDE_DIR
  NAMES lexbor/html/parser.h
  HINTS ${PC_Lexbor_INCLUDE_DIRS}
  PATH_SUFFIXES lexbor
)

find_library(Lexbor_LIBRARY
  NAMES lexbor
)

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
endif()

mark_as_advanced(Lexbor_INCLUDE_DIR Lexbor_LIBRARY)

