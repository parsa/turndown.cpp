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

# Include CTest only if testing is enabled
if(TURNDOWN_BUILD_TESTING)
    include(CTest)
endif()

# Add cmake directory to module path for FindGumbo.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Gumbo REQUIRED)

# Provide a reusable alias target for consumers
if(NOT TARGET turndown_cpp::gumbo)
    add_library(turndown_cpp::gumbo ALIAS Gumbo::Gumbo)
endif()

add_subdirectory(src)

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

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/turndown_cpp-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/turndown_cpp-config-version.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGumbo.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/turndown_cpp
)