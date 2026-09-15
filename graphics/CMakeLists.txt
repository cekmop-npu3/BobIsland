include(CMakePackageConfigHelpers)

add_library(graphics SHARED)
target_sources(graphics
    PRIVATE src/gui_win32.c
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES include/gui.h)
target_compile_definitions(graphics PRIVATE GRAPHICS_BUILD)
target_link_libraries(graphics PRIVATE project_options user32 gdi32 comctl32)
install(
    TARGETS
    graphics
    EXPORT
    GraphicsTargets
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
    GraphicsTargets
    NAMESPACE
    com_app::
    DESTINATION
    lib/cmake/graphics
)
configure_package_config_file(
    cmake/graphicsConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/graphicsConfig.cmake"
    INSTALL_DESTINATION
    lib/cmake/graphics
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/graphicsConfigVersion.cmake"
    VERSION
    "${PROJECT_VERSION}"
    COMPATIBILITY
    SameMajorVersion
)
install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/graphicsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/graphicsConfigVersion.cmake"
    DESTINATION
    lib/cmake/graphics
)
