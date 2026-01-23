# turndown.cpp/python/CMakeLists.txt

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(turndown MODULE
    src/bindings.cpp
)

target_link_libraries(turndown
    PRIVATE
        turndown_cpp_lib
)

if(DEFINED SKBUILD_PLATLIB_DIR)
    set(_turndown_py_install_dir "${SKBUILD_PLATLIB_DIR}")
else()
    include(GNUInstallDirs)
    set(_turndown_py_install_dir "${CMAKE_INSTALL_LIBDIR}")
endif()

install(TARGETS turndown
    LIBRARY DESTINATION "${_turndown_py_install_dir}" COMPONENT python
    RUNTIME DESTINATION "${_turndown_py_install_dir}" COMPONENT python
    ARCHIVE DESTINATION "${_turndown_py_install_dir}" COMPONENT python
)

