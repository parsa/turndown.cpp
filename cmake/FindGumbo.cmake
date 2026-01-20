# FindGumbo.cmake
# ----------------
# Find the Gumbo HTML5 parser library.
#
# This module uses pkg-config to locate Gumbo and creates an imported target.
#
# Imported Targets:
#   Gumbo::Gumbo - The Gumbo library
#
# Result Variables:
#   Gumbo_FOUND        - True if Gumbo was found
#   Gumbo_INCLUDE_DIRS - Include directories for Gumbo
#   Gumbo_LIBRARIES    - Libraries to link against
#   Gumbo_VERSION      - Version of Gumbo found
#
# Hints:
#   TURNDOWN_PREFER_STATIC - If set, prefer static libraries

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(_GUMBO QUIET gumbo)
endif()

find_path(Gumbo_INCLUDE_DIR
    NAMES gumbo.h
    HINTS ${_GUMBO_INCLUDE_DIRS}
    PATH_SUFFIXES gumbo
)

# Save and modify library suffixes to prefer static if requested
if(TURNDOWN_PREFER_STATIC)
    set(_Gumbo_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    if(WIN32)
        set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a)
    else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    endif()
endif()

find_library(Gumbo_LIBRARY
    NAMES gumbo
    HINTS ${_GUMBO_LIBRARY_DIRS}
)

# Restore original suffixes
if(TURNDOWN_PREFER_STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_Gumbo_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

if(_GUMBO_VERSION)
    set(Gumbo_VERSION "${_GUMBO_VERSION}")
endif()

find_package_handle_standard_args(Gumbo
    REQUIRED_VARS Gumbo_LIBRARY Gumbo_INCLUDE_DIR
    VERSION_VAR Gumbo_VERSION
)

if(Gumbo_FOUND)
    set(Gumbo_INCLUDE_DIRS "${Gumbo_INCLUDE_DIR}")
    set(Gumbo_LIBRARIES "${Gumbo_LIBRARY}")

    if(NOT TARGET Gumbo::Gumbo)
        add_library(Gumbo::Gumbo UNKNOWN IMPORTED)
        set_target_properties(Gumbo::Gumbo PROPERTIES
            IMPORTED_LOCATION "${Gumbo_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Gumbo_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(Gumbo_INCLUDE_DIR Gumbo_LIBRARY)

