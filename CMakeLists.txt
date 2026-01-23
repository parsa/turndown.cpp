cmake_minimum_required(VERSION 3.16)
project(turndown VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

# ============================================================================
# Build Options
# ============================================================================
# Centralized location for all project build options.
# All options use the TURNDOWN_ prefix for easy identification.

option(TURNDOWN_BUILD_CLI "Build the turndown CLI frontend" ON)
option(TURNDOWN_BUILD_EXAMPLES "Build the example executable" ON)
option(TURNDOWN_BUILD_TESTING "Build the test suite" ON)
option(TURNDOWN_BUILD_BENCHMARKS "Build Google Benchmark integration" ON)
option(TURNDOWN_BUILD_DOCS "Build Doxygen documentation" OFF)
option(TURNDOWN_BUILD_PYTHON "Build Python extension module (pybind11)" OFF)

# Parser backend selection
set(TURNDOWN_PARSER_BACKEND "gumbo" CACHE STRING "HTML parser backend (gumbo, tidy, lexbor, or libxml2)")
set_property(CACHE TURNDOWN_PARSER_BACKEND PROPERTY STRINGS "gumbo" "tidy" "lexbor" "libxml2")

# Static linking preference (for release builds)
option(TURNDOWN_PREFER_STATIC "Prefer static libraries for parser backend" OFF)

# Bundle libxml2 from source (primarily intended for Python wheel builds)
option(TURNDOWN_BUNDLE_LIBXML2 "Build libxml2 from source via FetchContent (static, no system dependency)" OFF)

# Include CTest only if testing is enabled
if(TURNDOWN_BUILD_TESTING)
    include(CTest)
endif()

# Add cmake directory to module path for Find*.cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find the selected parser backend
if(TURNDOWN_PARSER_BACKEND STREQUAL "gumbo")
    find_package(Gumbo REQUIRED)
    set(TURNDOWN_PARSER_TARGET Gumbo::Gumbo)
    set(TURNDOWN_PARSER_ADAPTER_SOURCE "gumbo_adapter.cpp")
elseif(TURNDOWN_PARSER_BACKEND STREQUAL "tidy")
    find_package(Tidy REQUIRED)
    set(TURNDOWN_PARSER_TARGET Tidy::Tidy)
    set(TURNDOWN_PARSER_ADAPTER_SOURCE "tidy_adapter.cpp")
elseif(TURNDOWN_PARSER_BACKEND STREQUAL "lexbor")
    find_package(Lexbor REQUIRED)
    set(TURNDOWN_PARSER_TARGET Lexbor::Lexbor)
    set(TURNDOWN_PARSER_ADAPTER_SOURCE "lexbor_adapter.cpp")
elseif(TURNDOWN_PARSER_BACKEND STREQUAL "libxml2")
    if(TURNDOWN_BUNDLE_LIBXML2)
        if(BUILD_SHARED_LIBS)
            message(FATAL_ERROR "TURNDOWN_BUNDLE_LIBXML2 requires BUILD_SHARED_LIBS=OFF (static build is required for wheel-friendly bundling).")
        endif()

        include(FetchContent)

        # Ensure libxml2 is built as a static library.
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

        # Configure libxml2 to build a minimal, static library suitable for bundling.
        set(LIBXML2_WITH_HTML ON CACHE BOOL "Enable HTML parser support" FORCE)
        set(LIBXML2_WITH_PROGRAMS OFF CACHE BOOL "Do not build xmllint/xmlcatalog" FORCE)
        set(LIBXML2_WITH_TESTS OFF CACHE BOOL "Do not build libxml2 tests" FORCE)
        set(LIBXML2_WITH_DOCS OFF CACHE BOOL "Do not build libxml2 docs" FORCE)
        set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "Do not build libxml2's own Python bindings" FORCE)
        set(LIBXML2_WITH_MODULES OFF CACHE BOOL "Disable dynamic modules" FORCE)
        set(LIBXML2_WITH_ZLIB OFF CACHE BOOL "Disable zlib dependency" FORCE)
        set(LIBXML2_WITH_ICONV OFF CACHE BOOL "Disable iconv dependency" FORCE)
        set(LIBXML2_WITH_ICU OFF CACHE BOOL "Disable ICU dependency" FORCE)
        set(LIBXML2_WITH_LEGACY OFF CACHE BOOL "Disable legacy mode" FORCE)

        FetchContent_Declare(
            libxml2
            GIT_REPOSITORY https://github.com/GNOME/libxml2.git
            GIT_TAG v2.15.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(libxml2)
    else()
        find_package(LibXml2 REQUIRED)
    endif()
    set(TURNDOWN_PARSER_TARGET LibXml2::LibXml2)
    set(TURNDOWN_PARSER_ADAPTER_SOURCE "libxml2_adapter.cpp")
else()
    message(FATAL_ERROR "Unknown parser backend: ${TURNDOWN_PARSER_BACKEND}. Use 'gumbo', 'tidy', 'lexbor', or 'libxml2'.")
endif()

message(STATUS "Using HTML parser backend: ${TURNDOWN_PARSER_BACKEND}")

# Compile definition for backend selection (used by internal implementation and tests).
string(TOUPPER "${TURNDOWN_PARSER_BACKEND}" TURNDOWN_PARSER_BACKEND_UPPER)
set(TURNDOWN_PARSER_BACKEND_DEFINE "TURNDOWN_PARSER_BACKEND_${TURNDOWN_PARSER_BACKEND_UPPER}=1")
add_compile_definitions(${TURNDOWN_PARSER_BACKEND_DEFINE})

# Provide a reusable alias target for consumers
if(NOT TARGET turndown_cpp::parser)
    # Use an IMPORTED INTERFACE wrapper so we can point at imported or ALIAS backends
    # without tripping CMake's ALIAS-of-ALIAS restrictions.
    add_library(turndown_cpp::parser INTERFACE IMPORTED)
    set_property(TARGET turndown_cpp::parser PROPERTY INTERFACE_LINK_LIBRARIES ${TURNDOWN_PARSER_TARGET})
endif()

add_subdirectory(src)

if(TURNDOWN_BUILD_PYTHON)
    add_subdirectory(python)
endif()

if(TURNDOWN_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(TURNDOWN_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(TURNDOWN_BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)

    if(TURNDOWN_BUILD_BENCHMARKS)
        find_package(benchmark QUIET)
        if(NOT benchmark_FOUND)
            message(WARNING "Google Benchmark requested but not found. Set TURNDOWN_BUILD_BENCHMARKS=OFF or install benchmark.")
        endif()
    endif()

    add_subdirectory(test)
endif()

# ============================================================================
# Documentation
# ============================================================================

if(TURNDOWN_BUILD_DOCS)
    find_package(Doxygen REQUIRED)

    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    # Optional: Install documentation
    install(DIRECTORY "${DOXYGEN_OUTPUT_DIR}/html/"
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        OPTIONAL
        COMPONENT documentation
    )
endif()

# ============================================================================
# Installation and export rules
# ============================================================================

include(CMakePackageConfigHelpers)

install(TARGETS turndown_cpp_lib
    EXPORT turndown_cpp_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TURNDOWN_BUILD_CLI)
    install(TARGETS turndown_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/turndown
    FILES_MATCHING PATTERN "*.h"
    PATTERN "gumbo_adapter.h" EXCLUDE
    PATTERN "tidy_adapter.h" EXCLUDE
    PATTERN "lexbor_adapter.h" EXCLUDE
    PATTERN "libxml2_adapter.h" EXCLUDE
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/turndown_cpp-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/turndown_cpp-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/turndown_cpp-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/turndown_cpp
)

install(EXPORT turndown_cpp_targets
    FILE turndown_cpp_targets.cmake
    NAMESPACE turndown::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/turndown_cpp
)

# Install config files and the appropriate Find module for the selected backend
set(TURNDOWN_FIND_MODULES)
if(TURNDOWN_PARSER_BACKEND STREQUAL "gumbo")
    list(APPEND TURNDOWN_FIND_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGumbo.cmake")
elseif(TURNDOWN_PARSER_BACKEND STREQUAL "tidy")
    list(APPEND TURNDOWN_FIND_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindTidy.cmake")
elseif(TURNDOWN_PARSER_BACKEND STREQUAL "lexbor")
    list(APPEND TURNDOWN_FIND_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindLexbor.cmake")
endif()

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/turndown_cpp-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/turndown_cpp-config-version.cmake"
    ${TURNDOWN_FIND_MODULES}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/turndown_cpp
)