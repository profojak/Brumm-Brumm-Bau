add_executable(brumm-brumm-bau
    src/main.cpp
    src/GameLayer.cpp
)

target_link_libraries(brumm-brumm-bau PUBLIC
    ptvc-engine
)

target_include_directories(brumm-brumm-bau PUBLIC
    ${PROJECT_SOURCE_DIR}/source/engine
    ${PROJECT_SOURCE_DIR}/source/shared
)

# Copy the "asset" directory next to the executable
add_custom_command(TARGET brumm-brumm-bau POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:brumm-brumm-bau>/assets
)
