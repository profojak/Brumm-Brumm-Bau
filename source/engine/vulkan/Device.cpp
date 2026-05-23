#include "Device.hpp"

#include <bitset>
#include <vulkan/vk_enum_string_helper.h>

#include "platform/platform.hpp"

namespace ptvc::rhi
{
    namespace detail
    {
        // As seen in: https://github.com/fknfilewalker/vulkan-triangle-modern/blob/sdl/src/main.cpp
        [[nodiscard]] static Option<uint32_t> findQueueFamilyIndex(const std::vector<vk::QueueFamilyProperties2>& queueFamiliesProperties, const vk::QueueFlags queueFlags) {
            std::optional<uint32_t> bestFamily;
            std::bitset<12> bestScore = 0;
            for (uint32_t i = 0; i < queueFamiliesProperties.size(); i++) {
                // check if queue family supports all requested queue flags
                if (static_cast<uint32_t>(queueFamiliesProperties[i].queueFamilyProperties.queueFlags & queueFlags) == static_cast<uint32_t>(queueFlags)) {
                    const std::bitset<12> score = static_cast<uint32_t>(queueFamiliesProperties[i].queueFamilyProperties.queueFlags);
                    // use queue family with the least other bits set
                    if (!bestFamily.has_value() || score.count() < bestScore.count()) {
                        bestFamily = i;
                        bestScore = score;
                    }
                }
            }
            return bestFamily;
        }

        [[nodiscard]] static bool checkHostImageCopySupport(const vk::PhysicalDevice& physicalDevice) noexcept
        {
            vk::PhysicalDeviceVulkan14Features vulkan14Features;
            auto features2 = vk::PhysicalDeviceFeatures2()
                .setPNext(&vulkan14Features);
            physicalDevice.getFeatures2(&features2);
            return vulkan14Features.hostImageCopy;
        }
    }

    Result<SPtr<Device>> Device::create(const DeviceCreateInfo& createInfo) noexcept
    {
        auto device = SPtr<Device>(std::move(new Device(createInfo)));
        if (const auto result = device->createDevice(); !result.has_value())
        {
            return std::unexpected(result.error());
        }
        return device;
    }

    void Device::waitIdle() const
    {
        mDevice.waitIdle();
    }

    const Queue& Device::getGraphicsQueue() const noexcept
    {
        return mGraphicsQueue;
    }

    VmaAllocator Device::getAllocator() const noexcept
    {
        return mAllocator;
    }

    vk::Device Device::getHandle() const noexcept
    {
        return mDevice;
    }

    vk::PhysicalDevice Device::getPhysicalDevice() const noexcept
    {
        return mPhysicalDevice;
    }

    Device::Device(const DeviceCreateInfo& createInfo)
    : mExtensions(createInfo.deviceExtensions)
    , mPhysicalDevice(createInfo.physicalDevice)
    , mInstance(createInfo.instance)
    {
    }

    Result<void> Device::createDevice() noexcept
    {
        /**
         * Check if HostImageCopy is available, even though it was promoted to Core 1.4 it is an optional feature.
         * https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_host_image_copy.html#_promotion_to_vulkan_1_4
         */
        mIsHostImageCopyAvailable = detail::checkHostImageCopySupport(mPhysicalDevice);

        #pragma region "Device and Vulkan Core 1.x Features"
        auto deviceFeatures = platform::getPhysicalDeviceFeatures();
        auto vulkan_1_1 = vk::PhysicalDeviceVulkan11Features();
        auto vulkan_1_2 = platform::getPhysicalDeviceVulkan12Features()
            .setPNext(&vulkan_1_1);
        auto vulkan_1_3 = vk::PhysicalDeviceVulkan13Features()
            .setMaintenance4(true)
            .setDynamicRendering(true)
            .setSynchronization2(true)
            .setInlineUniformBlock(true)
            .setPNext(&vulkan_1_2);
        auto vulkan_1_4 = vk::PhysicalDeviceVulkan14Features()
            .setMaintenance5(true)
            .setMaintenance6(true)
            .setHostImageCopy(mIsHostImageCopyAvailable)
            .setPNext(&vulkan_1_3);
        #pragma endregion

        // Queues
        const auto queueFamilyProperties = mPhysicalDevice.getQueueFamilyProperties2();
        constexpr float priority = 1.0f;

        const auto graphicsFamily = detail::findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eGraphics);
        if (!graphicsFamily.has_value())
        {
            exitWithError("No queue supporting the Graphics bit was found.");
        }

        std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos = {
            { {}, graphicsFamily.value(), 1, &priority },
        };

        const auto extensionNames = mExtensions.getActiveExtensionNames();

        auto createInfo = vk::DeviceCreateInfo()
            .setEnabledExtensionCount(extensionNames.size())
            .setPpEnabledExtensionNames(extensionNames.data())
            .setQueueCreateInfoCount(deviceQueueCreateInfos.size())
            .setPQueueCreateInfos(deviceQueueCreateInfos.data())
            .setPEnabledFeatures(&deviceFeatures)
            .setPNext(&vulkan_1_4);

        mExtensions.preDeviceCreation(createInfo);

        VK_CATCH(mDevice = mPhysicalDevice.createDevice(createInfo));

        // Get queues from the Device
        const auto graphicQueueIndex = graphicsFamily.value();
        const auto graphicsQueueInfo = vk::DeviceQueueInfo2()
            .setQueueFamilyIndex(graphicQueueIndex)
            .setQueueIndex(0);

        mGraphicsQueue = {
            .familyProperties = queueFamilyProperties[graphicQueueIndex],
            .familyIndex = graphicQueueIndex,
            .queues = { mDevice.getQueue2(graphicsQueueInfo) }
        };

        if (const auto result = createAllocator(); !result.has_value())
        {
            return std::unexpected(result.error());
        }

        mProperties2 = mPhysicalDevice.getProperties2();
        mDeviceName = std::string(mProperties2.properties.deviceName.data());

        return {};
    }

    Result<void> Device::createAllocator() noexcept
    {
        const VmaAllocatorCreateInfo createInfo = {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = mPhysicalDevice,
            .device = mDevice,
            .instance = mInstance,
            .vulkanApiVersion = VK_API_VERSION_1_4,
        };

        if (const auto result = vmaCreateAllocator(&createInfo, &mAllocator); result != VK_SUCCESS)
        {
            return std::unexpected(string_VkResult(result));
        }
        return {};
    }
}
