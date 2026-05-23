#include "Extensions.hpp"

#include <map>
#include <sstream>
#include <ranges>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/color.h>

namespace ptvc::rhi
{
    Extensions& Extensions::addExtension(const char* extensionName, const FeatureRequest requested) noexcept
    {
        if (mUniqueExtensionNames.contains(extensionName))
        {
            spdlog::warn("An extension by the name [{}] was already added!", styled(extensionName, fg(fmt::color::cyan)));
            return *this;
        }

        mUniqueExtensionNames.insert(extensionName);
        deviceExtensions.push_back(makeUnique<Extension>(extensionName, requested));
        return *this;
    }

    Extensions& Extensions::addPlatformRequiredExtensions() noexcept
    {
        #ifdef __APPLE__
        return addExtension(gVulkanPortabilitySubsetExtensionName, FeatureRequest::Required);
        #endif
        return *this;
    }

    bool Extensions::hasExtension(const char* extensionName) const noexcept
    {
        return contains(mActiveExtensionNames, extensionName);
    }

    int32_t Extensions::evaluateDeviceSupport(const vk::PhysicalDevice physicalDevice) const noexcept
    {
        const std::vector<vk::ExtensionProperties> driverExtensions = physicalDevice.enumerateDeviceExtensionProperties();

        int32_t                extensionScore = 0;
        std::map<size_t, bool> support        = {};
        for (auto&& [i, extension] : enumerate(deviceExtensions))
        {
            const auto requestType = extension->getRequestType();
            support[i] = containsIf(driverExtensions, [&extension](const vk::ExtensionProperties& properties) -> bool {
                return std::string_view{properties.extensionName.data()} == extension->getName();
            });
            // Scoring
            if (support[i])
            {
                switch (requestType)
                {
                    case FeatureRequest::Required: {
                        extensionScore += gDeviceScore_HasRequiredExtension;
                        break;
                    }
                    default: {
                        extensionScore += gDeviceScore_HasOptionalExtension;
                        break;
                    }
                }
            }
            else if (requestType == FeatureRequest::Required)
            {
                extensionScore += gDeviceScore_MissingRequiredExtension;
            }
        }

        return extensionScore;
    }

    void Extensions::postPhysicalDeviceSelection(const vk::PhysicalDevice physicalDevice) noexcept
    {
        const std::vector<vk::ExtensionProperties> driverExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        for (auto& extension : deviceExtensions)
        {
            const auto isSupported = containsIf(driverExtensions, [&extension](const vk::ExtensionProperties& properties) -> bool {
                return std::string_view{properties.extensionName.data()} == extension->getName();
            });
            extension->setSupported(isSupported);
        }

        // Save active extension names
        mActiveExtensionNames = deviceExtensions
            | std::views::filter([](const auto& ext){ return ext->isActive(); })
            | std::views::transform([](const auto& ext){ return ext->getName(); })
            | std::ranges::to<std::vector<const char*>>();
    }

    void Extensions::preDeviceCreation(vk::DeviceCreateInfo& deviceCreateInfo) const noexcept
    {
        for (const auto& extension : deviceExtensions)
        {
            extension->preCreateDevice(deviceCreateInfo);
        }
    }

    std::vector<const char*> Extensions::getExtensionNames() const noexcept
    {
        return mUniqueExtensionNames | std::ranges::to<std::vector<const char*>>();
    }

    const std::vector<const char*>& Extensions::getActiveExtensionNames() const noexcept
    {
        return mActiveExtensionNames;
    }

    std::string Extensions::toString() const noexcept
    {
        std::stringstream sstr;

        size_t w = 0;
        for (const auto& ext : deviceExtensions)
        {
            w = std::max(std::strlen(ext->getName()), w);
        }

        for (const auto& [i, extension] : enumerate(deviceExtensions))
        {
            if (i > 0)
            {
                sstr << std::endl;
            }
            sstr << "\t- " << extension->toString(w);
        }
        return sstr.str();
    }
}
