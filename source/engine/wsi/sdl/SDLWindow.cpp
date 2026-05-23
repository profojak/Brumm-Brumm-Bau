#include "SDLWindow.hpp"

#include <SDL3/SDL_vulkan.h>
#include <spdlog/fmt/bundled/color.h>

namespace ptvc::wsi
{
    SDLWindow::SDLWindow(const WindowCreateInfo& createInfo)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        {
            exitWithError("Failed to initialize windowing system.");
        }

        constexpr uint64_t windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

        const auto width  = static_cast<int32_t>(createInfo.size.width);
        const auto height = static_cast<int32_t>(createInfo.size.height);
        mWindow = SDL_CreateWindow(createInfo.title.data(), width, height, windowFlags);

        mDisplayScaling = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());;

        int32_t w, h;
        SDL_GetWindowSize(mWindow, &w, &h);
        mWindowSize = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };

        SDL_GetWindowSizeInPixels(mWindow, &w, &h);
        mFramebufferSize = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };

        spdlog::info("Created Window\n\t- Title: {}\n\t- Window Size: {}\n\t- Framebuffer Size: {}",
            createInfo.title, toString(mWindowSize), toString(mFramebufferSize));
    }

    SDLWindow::~SDLWindow()
    {
        if (mWindow)
        {
            SDL_DestroyWindow(mWindow);
        }
        SDL_Quit();
    }

    void SDLWindow::update_onResize() noexcept
    {
        int32_t w, h;
        SDL_GetWindowSize(mWindow, &w, &h);
        mWindowSize = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };

        SDL_GetWindowSizeInPixels(mWindow, &w, &h);
        mFramebufferSize = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    }

    SDL_Window* SDLWindow::getHandle() const noexcept
    {
        return mWindow;
    }

    vk::Extent2D SDLWindow::getFramebufferExtent() const noexcept
    {
        return { mFramebufferSize.width, mFramebufferSize.height };
    }

    Result<void> SDLWindow::createVulkanSurfaceKHR(const vk::Instance& instance,
        vk::SurfaceKHR* pSurface) const noexcept
    {
        if (!SDL_Vulkan_CreateSurface(mWindow, instance, nullptr, reinterpret_cast<VkSurfaceKHR*>(pSurface)))
        {
            return std::unexpected("Failed to create Vulkan Surface");
        }
        return {};
    }

    Result<std::vector<const char*>> SDLWindow::getRequiredInstanceExtensionsWSI() const noexcept
    {
        uint32_t   extensionCount;
        const auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (!extensions)
        {
            return std::unexpected("Failed to get Vulkan instance extensions required by GLFW.");
        }

        return {{extensions, extensions + extensionCount}};
    }

    float SDLWindow::getDisplayScale() const noexcept
    {
        return mDisplayScaling;
    }
}
