cmake_minimum_required(VERSION 3.24)
project(com_app VERSION 1.0.0 LANGUAGES C RC)

option(BUILD_TESTS "Build GoogleTest tests" ON)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE c_std_11 cxx_std_17)
target_compile_definitions(project_options INTERFACE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR
   CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(project_options INTERFACE /W4 /utf-8)
endif()

add_subdirectory(backend)
add_subdirectory(graphics)
add_subdirectory(app)
if(BUILD_TESTS)
    enable_testing()
    enable_language(CXX) # Only GoogleTest and test sources use C++.
    add_subdirectory(tests)
endif()
