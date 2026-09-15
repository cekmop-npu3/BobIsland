include(CMakePackageConfigHelpers)

add_library(backend_core STATIC)
target_sources(backend_core
    PRIVATE src/stream.c src/config_win32.c
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES include/stream.h
    PRIVATE FILE_SET internal_headers TYPE HEADERS
        BASE_DIRS src
        FILES src/config_win32.h)

add_library(backend SHARED)
target_sources(backend
    PRIVATE src/serial_win32.c
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES include/backend.h)
target_link_libraries(backend PRIVATE backend_core)
target_compile_definitions(backend PRIVATE BACKEND_BUILD)
target_link_libraries(backend_core PRIVATE project_options)
target_link_libraries(backend PRIVATE project_options)

install(
    TARGETS
    backend
    EXPORT
    BackendTargets
    RUNTIME
    DESTINATION
    .
    FILE_SET
    HEADERS
    DESTINATION
    include
)
install(
    EXPORT
    BackendTargets
    NAMESPACE
    com_app::
    DESTINATION
    lib/cmake/backend
)
configure_package_config_file(
    cmake/backendConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/backendConfig.cmake"
    INSTALL_DESTINATION
    lib/cmake/backend
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/backendConfigVersion.cmake"
    VERSION
    "${PROJECT_VERSION}"
    COMPATIBILITY
    SameMajorVersion
)
install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/backendConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/backendConfigVersion.cmake"
    DESTINATION
    lib/cmake/backend
)
