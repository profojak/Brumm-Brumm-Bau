find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

add_library(ptvc-vk
    VulkanContext.cpp
    VulkanContext.hpp
    platform/platform.hpp
    VulkanCore.hpp
    ext/Extension.hpp
    ext/Extension.cpp
    ext/Extensions.hpp
    ext/Extensions.cpp
    IWindow.hpp
    impl/vma.impl.cpp
    Device.hpp
    Device.cpp
    Swapchain.hpp
    Swapchain.cpp
    FrameSync.cpp
    FrameSync.hpp
    Buffer.hpp
    Buffer.cpp
    Image.hpp
    Image.cpp
    render/VertexType.hpp
    render/GraphicsPipeline.hpp
    render/GraphicsPipeline.cpp
    render/Pipeline.hpp
    Descriptor.hpp
    Descriptor.cpp
    render/Pipeline.cpp
    render/Shaders.hpp
    render/Shaders.cpp
    VulkanContextOptions.hpp
)

target_link_libraries(ptvc-vk
    PUBLIC ${Vulkan_LIBRARIES} spdlog::spdlog GPUOpen::VulkanMemoryAllocator Vulkan::shaderc_combined
)

target_include_directories(ptvc-vk
    PUBLIC ${Vulkan_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/external/spdlog/include
    ${PROJECT_SOURCE_DIR}/external/VulkanMemoryAllocator/include
    ${PROJECT_SOURCE_DIR}/source/shared
    ${PROJECT_SOURCE_DIR}/source/engine/vulkan
    ${PROJECT_SOURCE_DIR}/source/shared
)

target_compile_definitions(ptvc-vk
    PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)
