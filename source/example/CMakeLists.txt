add_executable(ptvc-example-project
    src/main.cpp
    src/RotatingGameObject.hpp
    src/ControllableGameObject.hpp
    src/GameLayer.hpp
    src/ControllableGameObject.cpp
    src/RotatingGameObject.cpp
    src/GameLayer.cpp
)

target_link_libraries(ptvc-example-project PUBLIC
    ptvc-engine
)

target_include_directories(ptvc-example-project PUBLIC
    ${PROJECT_SOURCE_DIR}/source/engine
    ${PROJECT_SOURCE_DIR}/source/shared
)

# Copy the "asset" directory next to the executable
add_custom_command(TARGET ptvc-example-project POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:ptvc-example-project>/assets
)

