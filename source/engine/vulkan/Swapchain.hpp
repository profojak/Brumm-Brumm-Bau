#pragma once

#include <vector>
#include "VulkanCore.hpp"

namespace ptvc::rhi
{
    class Device;
    class IWindow;

    struct SwapchainCreateInfo
    {
        SPtr<Device>     device;
        SPtr<IWindow>    window;
        vk::SurfaceKHR   surface;
        class Swapchain* pOldSwapchain = nullptr;
    };

    class Swapchain
    {
    public:
        DISABLE_COPY(Swapchain);

        [[nodiscard]] static Result<UPtr<Swapchain>> create(const SwapchainCreateInfo& createInfo) noexcept;

        ~Swapchain();

        /**
         * Create an ImageMemoryBarrier for the given image based on the tracked state and given dst state.\n
         * Updates the internally tracked state on function call.
         * @param i Acquired Swapchain image index
         * @param dstState Desired state transition for the image
         * @return Filled in vk::ImageMemoryBarrier
         */
        [[nodiscard]] vk::ImageMemoryBarrier2 getBarrier(uint32_t i, const ImageState& dstState) noexcept;

        [[nodiscard]] vk::Image getImage(uint32_t i) const noexcept;
        [[nodiscard]] vk::ImageView getImageView(uint32_t i) const noexcept;

        [[nodiscard]] vk::SwapchainKHR getHandle() const noexcept;
        [[nodiscard]] uint32_t getImageCount() const noexcept;
        [[nodiscard]] vk::Format getFormat() const noexcept;
        [[nodiscard]] vk::Extent2D getExtent() const noexcept;

        [[nodiscard]] vk::Rect2D getScissor() const noexcept
        {
            return {{ 0, 0 }, mExtent};
        }

        [[nodiscard]] vk::Viewport getViewport() const noexcept
        {
            return vk::Viewport()
                .setX(0.0f)
                .setY(0.0f)
                .setWidth(static_cast<float>(mExtent.width))
                .setHeight(static_cast<float>(mExtent.height))
                .setMaxDepth(1.0f)
                .setMinDepth(0.0f);
        }

    private:
        explicit Swapchain(const SwapchainCreateInfo& createInfo);

        [[nodiscard]] Result<void> createSwapchain(vk::SurfaceKHR surface, const Swapchain* pOldSwapchain = nullptr) noexcept;

        vk::Format                      mFormat;
        vk::Extent2D                    mExtent;

        std::vector<vk::ImageView>      mImageViews;
        std::vector<vk::Image>          mImages;
        std::vector<ImageState>         mImageStates;
        vk::SwapchainKHR                mSwapchain;
        uint32_t                        mImageCount = 2u;

        SPtr<IWindow>                   mWindow;
        SPtr<Device>                    mDevice;
    };
}
