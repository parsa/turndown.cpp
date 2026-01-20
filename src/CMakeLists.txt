# turndown.cpp/src/CMakeLists.txt

add_library(turndown_cpp_lib
    turndown.cpp
    rules.cpp
    commonmark_rules.cpp
    collapse_whitespace.cpp
    node.cpp
    utilities.cpp
    dom_adapter.cpp
    dom_source.cpp
    ${TURNDOWN_PARSER_ADAPTER_SOURCE}
)

target_include_directories(turndown_cpp_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/turndown>
)

target_link_libraries(turndown_cpp_lib
    PUBLIC
        turndown_cpp::parser
)

set_target_properties(turndown_cpp_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
