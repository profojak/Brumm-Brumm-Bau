add_subdirectory(vulkan)

set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/external/imgui)

add_library(ptvc-engine
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui.h ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp

    impl/stb.impl.cpp

    core/Application.cpp
    core/Application.hpp
    wsi/WindowCreateInfo.hpp
    wsi/sdl/SDLWindow.hpp
    wsi/sdl/SDLWindow.cpp
    core/Layer.hpp
    ui/ImGuiIntegration.hpp
    ui/ImGuiIntegration.cpp
    scene/Scene.hpp
    scene/Transform.hpp
    scene/Geometry.hpp
    scene/primitives/Cube.hpp
    scene/Vertex.hpp
    scene/primitives/Cube.cpp
    scene/Geometry.cpp
    scene/ICamera.hpp
    scene/GameObject.hpp
    scene/Scene.cpp
    render/DebugLayer.hpp
    render/DebugLayer.cpp
    scene/FreeCamera.hpp
    scene/FreeCamera.cpp
    core/IO.hpp
    core/IO.cpp
    core/ApplicationOptions.hpp
    scene/Lights.hpp
    physics/Physics.hpp
    physics/Physics.cpp
)

target_link_libraries(ptvc-engine
    PUBLIC ptvc-vk glm::glm SDL3::SDL3 spdlog Jolt
)

target_include_directories(ptvc-engine
    PUBLIC ./
    ${PROJECT_SOURCE_DIR}/external/glm
    ${PROJECT_SOURCE_DIR}/external/imgui
    ${PROJECT_SOURCE_DIR}/external/sdl/include
    ${PROJECT_SOURCE_DIR}/external/spdlog/include
    ${PROJECT_SOURCE_DIR}/external/stb
)

target_compile_definitions(ptvc-engine PUBLIC
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    -DImTextureID=ImU64
)
