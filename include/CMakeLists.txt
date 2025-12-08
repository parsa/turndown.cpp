# turndown.cpp/include/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(turndown_cpp_include)

# Headers are installed by the parent CMakeLists.txt using install(DIRECTORY ...)
# No targets or specific configurations are needed here for this simple include directory.

# You could potentially add install rules here if you want finer control over header installation,
# but for now, the install(DIRECTORY ...) in the root CMakeLists.txt is sufficient.