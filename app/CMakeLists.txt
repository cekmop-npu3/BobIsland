add_executable(com_app WIN32)
target_sources(
    com_app
    PRIVATE
    src/main.c
    resources/app.manifest
)
target_link_libraries(com_app PRIVATE graphics backend user32)
target_link_libraries(com_app PRIVATE project_options)
install(TARGETS com_app RUNTIME DESTINATION .)
