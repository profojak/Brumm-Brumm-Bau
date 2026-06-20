add_subdirectory(vulkan)

set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/external/imgui)

add_library(ptvc-engine
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui.h ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp

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
    scene/Terrain.hpp
    scene/Terrain.cpp
    render/DebugLayer.hpp
    render/DebugLayer.cpp
    render/EdgeDetect.hpp
    render/EdgeDetect.cpp
    scene/FreeCamera.hpp
    scene/FreeCamera.cpp
    scene/OrbitCamera.hpp
    scene/OrbitCamera.cpp
    core/IO.hpp
    core/IO.cpp
    core/ApplicationOptions.hpp
    scene/Lights.hpp
    scene/ShadowMap.hpp
    scene/ShadowMap.cpp
    physics/Physics.hpp
    physics/Physics.cpp
    physics/TerrainPhysics.hpp
    physics/TerrainPhysics.cpp
    physics/VehiclePhysics.hpp
    physics/VehiclePhysics.cpp
    impl/stb.impl.cpp
    impl/cgltf.impl.cpp
    scene/glTF.hpp
    scene/glTF.cpp
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
    ${PROJECT_SOURCE_DIR}/external/cgltf
)

target_compile_definitions(ptvc-engine PUBLIC
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    -DImTextureID=ImU64
)
