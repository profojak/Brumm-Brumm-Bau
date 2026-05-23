#pragma once

#include <set>
#include <string>

#include "Extension.hpp"
#include "VulkanCore.hpp"
#include "lib/lib.hpp"

namespace ptvc::rhi {
/**
     * @brief A class to manage Vulkan Extensions
     */
struct Extensions
{
  std::vector<UPtr<Extension>> deviceExtensions;

  // Register basic extension
  Extensions& addExtension(const char* extensionName, FeatureRequest requested) noexcept;

  // Register subclassed extension
  template <class T>
    requires std::is_base_of_v<Extension, T>
  Extensions& addExtension(FeatureRequest requested) noexcept
  {
    auto extension = makeUnique<T>(requested);
    if(mUniqueExtensionNames.contains(extension->getName()))
    {
      spdlog::warn("An extension by the name [{}] was already added!", styled(extension->getName(), fg(fmt::color::cyan)));
      return *this;
    }
    mUniqueExtensionNames.insert(extension->getName());
    deviceExtensions.push_back(std::move(extension));
    return *this;
  }

  // Register platform required extensions
  Extensions& addPlatformRequiredExtensions() noexcept;

  [[nodiscard]] bool hasExtension(const char* extensionName) const noexcept;

  /**
         * Evaluate extension support and compute a score for the specified PhysicalDevice
         * @note If the device is missing it receives a big enough penalty for the score to always be negative.
         * If optional or disabled features are present the device will receive some extra points.
         * @param physicalDevice
         * @return Returns the "extension score" of the specified PhysicalDevice
         */
  [[nodiscard]] int32_t evaluateDeviceSupport(vk::PhysicalDevice physicalDevice) const noexcept;

  /**
         * Toggle the supported status for each extension.
         * @note Should be used after a PhysicalDevice has been selected.
         * @param physicalDevice
         */
  void postPhysicalDeviceSelection(vk::PhysicalDevice physicalDevice) noexcept;

  /**
         * For each extension, do the pre-device creation steps
         * e.g. adding feature struts to the pNext chain.
         * @note Should be used before calling vk::PhysicalDevice::createDevice()
         */
  void preDeviceCreation(vk::DeviceCreateInfo& deviceCreateInfo) const noexcept;

  [[nodiscard]] std::vector<const char*> getExtensionNames() const noexcept;

  [[nodiscard]] const std::vector<const char*>& getActiveExtensionNames() const noexcept;

  [[nodiscard]] std::string toString() const noexcept;

private:
  std::set<const char*> mUniqueExtensionNames;

  // ! Only valid after "postPhysicalDeviceSelection" has been called !
  std::vector<const char*> mActiveExtensionNames;
};
}  // namespace ptvc::rhi
