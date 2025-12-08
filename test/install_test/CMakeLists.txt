cmake_minimum_required(VERSION 3.16)
project(turndown_install_test CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(turndown_cpp REQUIRED)

add_executable(install_test main.cpp)
target_link_libraries(install_test PRIVATE turndown::turndown_cpp_lib)

