#pragma once

#include <lib/platform.hpp>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

namespace ptvc::rhi::platform
{
    // Get the Instance flags required by the current platform.
    [[nodiscard]] constexpr vk::InstanceCreateFlags getInstanceFlags() noexcept
    {
        // If a platform supports the Portability Enumeration extension this flag must be set.
        // Vulkan on macOS requires this as it's only supported via translation layers.
        if constexpr (isApple)
        {
            return vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }

        return {};
    }

    // Get the list of Instance extensions required by the current platform.
    [[nodiscard]] constexpr std::vector<const char*> getInstanceExtensions() noexcept
    {
        // Platforms supporting this extension must use it.
        if constexpr (isApple)
        {
            return { vk::KHRPortabilityEnumerationExtensionName };
        }

        return {};
    }

    [[nodiscard]] inline vk::PhysicalDeviceFeatures getPhysicalDeviceFeatures() noexcept
    {
        constexpr auto result = vk::PhysicalDeviceFeatures()
            .setGeometryShader(!isApple)
            .setTessellationShader(true)
            .setMultiDrawIndirect(true)
            .setDrawIndirectFirstInstance(true)
            .setFillModeNonSolid(true)
            .setSamplerAnisotropy(true)
            .setSampleRateShading(true)
            .setShaderInt64(true);

        if constexpr (isApple)
        {
            spdlog::warn("{}", STYLE_WARNING("Geometry shaders are not available on macOS"));
        }

        return result;
    }

    [[nodiscard]] inline vk::PhysicalDeviceVulkan12Features getPhysicalDeviceVulkan12Features() noexcept
    {
        constexpr auto result = vk::PhysicalDeviceVulkan12Features()
            .setBufferDeviceAddress(true)
            .setDescriptorIndexing(true)
            .setScalarBlockLayout(true)
            .setShaderInt8(true)
            .setTimelineSemaphore(true)
            .setHostQueryReset(true)
            .setScalarBlockLayout(true)
            .setDrawIndirectCount(!isApple);

        if constexpr (isApple)
        {
            spdlog::warn("{}", STYLE_WARNING("DrawIndirectCount (Vulkan Core 1.2) is not available on macOS"));
        }

        return result;
    }
}
