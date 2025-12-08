add_executable(turndown_example main.cpp)

# Link the example executable to the turndown_cpp_lib library
target_link_libraries(turndown_example PRIVATE turndown_cpp_lib)

# Optionally set install target for the example, if you want to install it.
# install(TARGETS turndown_example DESTINATION bin)