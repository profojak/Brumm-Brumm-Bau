#pragma once

#include "VulkanCore.hpp"

namespace ptvc::rhi
{
    /**
     * Windows that will be used for a Swapchain must implement this interface.
     * @note The primary purpose of this interface is so that the Vulkan wrapper can be independent
     * of the used windowing system integration.
     */
    class IWindow
    {
    public:
        virtual ~IWindow() = default;

        /**
         * Return the size of the Windows framebuffer.
         * @note This value should be used for Swapchain creation.
         */
        virtual vk::Extent2D getFramebufferExtent() const noexcept = 0;

        /**
         * WSI libraries might have a function for creating a Vulkan surface.
         * e.g. glfwCreateWindowSurface(), SDL_Vulkan_CreateSurface()
         */
        virtual Result<void> createVulkanSurfaceKHR(const vk::Instance& instance, vk::SurfaceKHR* pSurface) const noexcept = 0;

        /**
         * WSI libraries might have a function that return the required Vulkan Instance extensions.
         * e.g. glfwGetRequiredInstanceExtensions(), SDL_Vulkan_GetInstanceExtensions()
         */
        virtual Result<std::vector<const char*>> getRequiredInstanceExtensionsWSI() const noexcept = 0;
    };
}
