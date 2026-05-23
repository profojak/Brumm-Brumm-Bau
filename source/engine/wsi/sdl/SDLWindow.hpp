#pragma once

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include "vulkan/IWindow.hpp"
#include "wsi/WindowCreateInfo.hpp"

namespace ptvc::wsi
{
    class SDLWindow : public rhi::IWindow
    {
    public:
        explicit SDLWindow(const WindowCreateInfo& createInfo);

        ~SDLWindow() override;

        /**
         * Update cached window size parameters after resizing.
         */
        void update_onResize() noexcept;

        [[nodiscard]] SDL_Window* getHandle() const noexcept;

        [[nodiscard]] float getDisplayScale() const noexcept;

        [[nodiscard]] vk::Extent2D getFramebufferExtent() const noexcept override;

        [[nodiscard]] Result<void> createVulkanSurfaceKHR(const vk::Instance& instance, vk::SurfaceKHR* pSurface) const noexcept override;

        [[nodiscard]] Result<std::vector<const char*>> getRequiredInstanceExtensionsWSI() const noexcept override;

    private:
        float       mDisplayScaling;

        Size2D      mWindowSize;
        Size2D      mFramebufferSize;
        SDL_Window* mWindow = nullptr;
    };
}
