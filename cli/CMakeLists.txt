add_executable(turndown_cli main.cpp)
target_link_libraries(turndown_cli PRIVATE turndown_cpp_lib)

# dlopen/dlsym live in libdl on many Unix platforms (e.g. Linux).
if(CMAKE_DL_LIBS)
    target_link_libraries(turndown_cli PRIVATE ${CMAKE_DL_LIBS})
endif()
