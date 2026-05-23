#pragma once

#include <expected>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include <lib/lib.hpp>

namespace ptvc::rhi
{
    // Vulkan layer and extension names without definitions
    constexpr auto      gVulkanKhronosValidationLayerName       = "VK_LAYER_KHRONOS_validation";
    constexpr auto      gVulkanPortabilitySubsetExtensionName   = "VK_KHR_portability_subset";

    // Vendor ID constants
    constexpr uint32_t  gVendorID_Apple                         = 0x106b;
    constexpr uint32_t  gVendorID_AMD                           = 0x1002;
    constexpr uint32_t  gVendorID_Intel                         = 0x8086;
    constexpr uint32_t  gVendorID_Nvidia                        = 0x10de;

    // PhysicalDevice scoring values for selection
    constexpr int32_t   gDeviceScore_MissingRequiredExtension   = -10000000;
    constexpr int32_t   gDeviceScore_HasRequiredExtension       =  100000;
    constexpr int32_t   gDeviceScore_HasOptionalExtension       =  10000;
    constexpr int32_t   gDeviceScore_IsDedicatedGPU             =  1000000;
    constexpr int32_t   gDeviceScore_IsIntegratedGPU            =  10000;

    #define VK_CATCH(EXPR)                      \
        try { EXPR; }                           \
        catch (const vk::SystemError& e) {      \
            return std::unexpected(e.what());   \
        }

    #define VK_RESULT(RESULT)                           \
        if (RESULT != vk::Result::eSuccess) {           \
            return std::unexpected(to_string(RESULT));  \
        }

    #define VK_CHECK(RESULT)                        \
        if (RESULT != vk::Result::eSuccess) {       \
            return exitWithError("Error: {}",       \
                STYLE_ERROR(to_string(RESULT)));    \
        }

    // Objects and data for one Frame
    struct Frame
    {
        vk::CommandBuffer   commandBuffer;
        vk::Fence           fPresentFinished;
        vk::Semaphore       sImageAvailable;
        vk::Semaphore       sRenderingFinished;
        uint32_t            currentFrameIndex;
        uint32_t            acquiredImageIndex;
    };

    // Struct for tracking image state (used for the Swapchain, single mip level)
    struct ImageState
    {
        vk::ImageLayout         layout     = vk::ImageLayout::eUndefined;
        vk::AccessFlags2        accessMask = vk::AccessFlagBits2::eNone;
        vk::PipelineStageFlags2 stageMask  = vk::PipelineStageFlagBits2::eNone;
    };

    // Immutable properties of an Image
    struct ImageProperties
    {
        vk::Format                  format;
        vk::Extent2D                extent;
        vk::ImageAspectFlags        aspectFlags;
        uint32_t                    levelCount;
        vk::SampleCountFlagBits     samples;

        vk::ImageSubresourceRange   maxSubresourceRange;
        vk::ImageSubresourceLayers  maxSubresourceLayers;
    };

    // Contains queue family info and the queue handles.
    struct Queue
    {
        vk::QueueFamilyProperties2  familyProperties    = {};
        uint32_t                    familyIndex         = 0u;
        std::vector<vk::Queue>      queues;

        [[nodiscard]] vk::Queue getHandle(const size_t i = 0) const noexcept
        {
            assert(i < queues.size());
            return queues[0];
        }
    };

    /**
     * Check whether all the required Vulkan layers or extensions are supported or not.
     * @tparam T Vulkan Layer or Extension Properties
     * @param availableFeatures List of T
     * @param requiredFeatures List of the names of the required features.
     * @return True on success or the list of missing features if not all required ones are supported.
     */
    template <class T>
    [[nodiscard]] Result<bool, std::vector<const char*>> evaluateFeatureSupport(const std::vector<T>& availableFeatures, const std::vector<const char*>& requiredFeatures) noexcept
    {
        static_assert(std::is_same_v<vk::LayerProperties, T> || std::is_same_v<vk::ExtensionProperties, T>, "T must be either Layer or ExtensionProperties");

        std::vector<const char*> missingFeatures;
        const auto allSupported = std::ranges::all_of(requiredFeatures, [&availableFeatures, &missingFeatures](const char* name) -> bool {
            const auto it = std::ranges::find_if(availableFeatures, [name](const T& properties) -> bool {
                if constexpr (std::is_same_v<vk::LayerProperties, T>)
                {
                    return std::string_view{ properties.layerName.data() } == name;
                }
                else if constexpr (std::is_same_v<vk::ExtensionProperties, T>)
                {
                    return std::string_view{ properties.extensionName.data() } == name;
                }
                return false;
            });

            if (it == std::end(availableFeatures))
            {
                missingFeatures.push_back(name);
            }
            return it != std::end(availableFeatures);
        });

        if (!missingFeatures.empty())
        {
            return std::unexpected(missingFeatures);
        }

        return allSupported;
    }
}
