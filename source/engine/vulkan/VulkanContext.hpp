#pragma once

#include <functional>
#include "Device.hpp"
#include "FrameSync.hpp"
#include "Swapchain.hpp"
#include "VulkanContextOptions.hpp"
#include "VulkanCore.hpp"
#include "ext/Extensions.hpp"

namespace ptvc::rhi
{
    class IWindow;

    struct VulkanContextCreateInfo
    {
        SPtr<IWindow>        window  = nullptr;
        VulkanContextOptions options = {};
    };

    /**
     * Class for interaction with the Vulkan API.
     */
    class VulkanContext
    {
    public:
        DISABLE_COPY(VulkanContext);

        [[nodiscard]] static SPtr<VulkanContext> create(const VulkanContextCreateInfo& createInfo) noexcept;

        /**
         * Acquire the next Swapchain Image and begin the next frame.\n
         * By default, a CommandBuffer is allocated from the default CommandPool, which is created
         * using the default (graphics) queue family.
         * @return Frame object
         */
        [[nodiscard]] Result<Frame> beginFrame() const noexcept;

        /**
         * Submit the frame CommandBuffer and present the Swapchain.
         * (Also frees the CommandBuffer allocated by beginFrame().)
         * @param frame Frame object received from beginFrame()
         */
        void endFrame_submitAndPresent(const Frame& frame) const noexcept;

        /**
         * Record a single-time CommandBuffer and execute it immediately on the default queue.
         * @param lambda
         */
        void executeImmediateCommand(const std::function<void (const vk::CommandBuffer&)>& lambda) const noexcept;

        void rebuildSwapchain() noexcept;

        [[nodiscard]] SPtr<Device> getDevice() const noexcept;

        [[nodiscard]] Swapchain* getSwapchain() const noexcept;

        [[nodiscard]] vk::Instance getInstance() const noexcept;

    private:
        explicit VulkanContext(const VulkanContextCreateInfo& createInfo);

        [[nodiscard]] Result<void> createVulkanInstance() noexcept;

        [[nodiscard]] Result<void> createDebugMessenger() noexcept;

        [[nodiscard]] Result<vk::PhysicalDevice> selectPhysicalDevice() const noexcept;

        void createSwapchain() noexcept;

        UPtr<FrameSync>                 mFrameSync;

        vk::CommandPool                 mCommandPool;
        UPtr<Swapchain>                 mSwapchain;
        SPtr<Device>                    mDevice;

        vk::PhysicalDevice              mPhysicalDevice;
        vk::PhysicalDeviceProperties2   mPhysicalDeviceProperties;
        Extensions                      mDeviceExtensions;

        vk::SurfaceKHR                  mSurface;

        vk::DebugUtilsMessengerEXT      mDebugMessenger;
        vk::Instance                    mInstance;

        SPtr<IWindow>                   mWindow;
    };
}
