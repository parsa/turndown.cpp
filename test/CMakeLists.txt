# turndown.cpp/test/CMakeLists.txt

set(TURNDOWN_GTEST_TIMEOUT 60)

function(turndown_add_gtest target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE
        turndown_cpp_lib
        GTest::gtest_main
    )
    # Keep per-test discovery, but don't run the executable at build time on
    # Windows (DLL dependencies are easier to satisfy at test time).
    if(WIN32)
        gtest_discover_tests(${target}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DISCOVERY_TIMEOUT ${TURNDOWN_GTEST_TIMEOUT}
            DISCOVERY_MODE PRE_TEST
            NO_PRETTY_VALUES TRUE
        )
    else()
        gtest_discover_tests(${target}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DISCOVERY_TIMEOUT ${TURNDOWN_GTEST_TIMEOUT}
            DISCOVERY_MODE POST_BUILD
            NO_PRETTY_VALUES TRUE
        )
    endif()
endfunction()

turndown_add_gtest(turndown_test turndown_test.cpp)
turndown_add_gtest(internals_test internals_test.cpp)

if(TURNDOWN_BUILD_CLI)
    turndown_add_gtest(turndown_cli_test cli_test.cpp)
    target_compile_definitions(turndown_cli_test PRIVATE
        TURNDOWN_CLI_PATH="$<TARGET_FILE:turndown_cli>"
    )
    add_dependencies(turndown_cli_test turndown_cli)
else()
    message(STATUS "Skipping CLI integration tests because TURNDOWN_BUILD_CLI is OFF.")
endif()

set(_turndown_has_benchmark FALSE)
if(TURNDOWN_BUILD_BENCHMARKS AND DEFINED benchmark_FOUND AND benchmark_FOUND)
    set(_turndown_has_benchmark TRUE)
endif()

turndown_add_gtest(turndown_index_test turndown_index_test.cpp)
if(_turndown_has_benchmark)
    target_link_libraries(turndown_index_test PRIVATE benchmark::benchmark)
    target_compile_definitions(turndown_index_test PRIVATE TURNDOWN_HAS_BENCHMARK=1)
else()
    target_compile_definitions(turndown_index_test PRIVATE TURNDOWN_HAS_BENCHMARK=0)
    message(STATUS "Building turndown_index_test without Google Benchmark support.")
endif()