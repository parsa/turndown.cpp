# FindTidy.cmake
# Find the tidy-html5 library
#
# This module defines the following variables:
#   Tidy_FOUND        - True if tidy was found
#   Tidy_INCLUDE_DIRS - Include directories for tidy
#   Tidy_LIBRARIES    - Libraries to link against
#   Tidy_VERSION      - Version of tidy found
#
# This module defines the following imported targets:
#   Tidy::Tidy        - The tidy library
#
# Hints:
#   TURNDOWN_PREFER_STATIC - If set, prefer static libraries

include(FindPackageHandleStandardArgs)

# Try pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_Tidy QUIET tidy)
endif()

# Find include directory
find_path(Tidy_INCLUDE_DIR
    NAMES tidy.h
    HINTS
        ${PC_Tidy_INCLUDEDIR}
        ${PC_Tidy_INCLUDE_DIRS}
    PATH_SUFFIXES tidy
)

# Save and modify library suffixes to prefer static if requested
if(TURNDOWN_PREFER_STATIC)
    set(_Tidy_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    if(WIN32)
        set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a)
    else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    endif()
endif()

# Find library
find_library(Tidy_LIBRARY
    NAMES tidy tidy5 tidys tidy5s
    HINTS
        ${PC_Tidy_LIBDIR}
        ${PC_Tidy_LIBRARY_DIRS}
)

# Restore original suffixes
if(TURNDOWN_PREFER_STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_Tidy_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# Get version from header if possible
if(Tidy_INCLUDE_DIR AND EXISTS "${Tidy_INCLUDE_DIR}/tidyplatform.h")
    file(STRINGS "${Tidy_INCLUDE_DIR}/tidyplatform.h" Tidy_VERSION_LINE
         REGEX "^#define[ \t]+LIBTIDY_VERSION[ \t]+\"[^\"]+\"")
    if(Tidy_VERSION_LINE)
        string(REGEX REPLACE "^#define[ \t]+LIBTIDY_VERSION[ \t]+\"([^\"]+)\".*" "\\1"
               Tidy_VERSION "${Tidy_VERSION_LINE}")
    endif()
endif()

find_package_handle_standard_args(Tidy
    REQUIRED_VARS Tidy_LIBRARY Tidy_INCLUDE_DIR
    VERSION_VAR Tidy_VERSION
)

if(Tidy_FOUND)
    set(Tidy_INCLUDE_DIRS ${Tidy_INCLUDE_DIR})
    set(Tidy_LIBRARIES ${Tidy_LIBRARY})

    if(NOT TARGET Tidy::Tidy)
        add_library(Tidy::Tidy UNKNOWN IMPORTED)
        set_target_properties(Tidy::Tidy PROPERTIES
            IMPORTED_LOCATION "${Tidy_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Tidy_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(Tidy_INCLUDE_DIR Tidy_LIBRARY)

