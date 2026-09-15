include(FetchContent)
string(CONCAT googletest_url
    "https://github.com/google/googletest/archive/"
    "03597a01ee50ed33e9dfd640b249b4be3799d395.zip"
)
FetchContent_Declare(
    googletest
    URL ${googletest_url}
)
# For Windows: prevent GoogleTest from overriding the parent compiler/linker
# settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(core_tests)
target_sources(core_tests PRIVATE stream_test.cpp)
target_link_libraries(
    core_tests
    PRIVATE
    project_options
    backend_core
    GTest::gtest_main
)
add_test(NAME core_tests COMMAND core_tests)

add_executable(windows_tests)
target_sources(windows_tests
    PRIVATE config_test.cpp serial_test.cpp
    PRIVATE FILE_SET HEADERS
        BASE_DIRS ../backend/src
        FILES ../backend/src/config_win32.h)
target_link_libraries(
    windows_tests
    PRIVATE
    project_options
    backend
    backend_core
    GTest::gtest_main
)
add_test(NAME windows_tests COMMAND windows_tests)

add_executable(gui_tests)
target_sources(gui_tests PRIVATE gui_test.cpp)
target_link_libraries(
    gui_tests
    PRIVATE
    project_options
    graphics
    user32
    GTest::gtest_main
)
add_test(NAME gui_tests COMMAND gui_tests)
