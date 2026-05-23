#pragma once

#include <vk_mem_alloc.h>

#include "VulkanCore.hpp"
#include "ext/Extensions.hpp"

namespace ptvc::rhi {
struct DeviceCreateInfo
{
  vk::Instance       instance;
  vk::PhysicalDevice physicalDevice;
  Extensions&        deviceExtensions;
};

template <class T>
struct ObjectNameInfo
{
  std::string_view name;
  T                handle;
};

class Device
{
public:
  DISABLE_COPY(Device);

  [[nodiscard]] static Result<SPtr<Device>> create(const DeviceCreateInfo& createInfo) noexcept;

  void waitIdle() const;

  [[nodiscard]] const Queue& getGraphicsQueue() const noexcept;

  /**
         * Set the (debug) Object Name of a Vulkan resource.
         * @tparam T Vulkan object type
         * @param objectNameInfo Handle and Name
         */
  template <class T>
  void setLabel(const ObjectNameInfo<T>& objectNameInfo) const;

  [[nodiscard]] VmaAllocator getAllocator() const noexcept;

  [[nodiscard]] vk::Device getHandle() const noexcept;

  [[nodiscard]] vk::PhysicalDevice getPhysicalDevice() const noexcept;

  const std::string& getName() const noexcept { return mDeviceName; }

  [[nodiscard]] bool isHostImageCopyAvailable() const noexcept { return mIsHostImageCopyAvailable; }

private:
  explicit Device(const DeviceCreateInfo& createInfo);

  Result<void> createDevice() noexcept;

  Result<void> createAllocator() noexcept;

  bool mEnableObjectLabeling     = false;
  bool mIsHostImageCopyAvailable = false;

  vk::Device   mDevice;
  Queue        mGraphicsQueue;
  VmaAllocator mAllocator{};

  Extensions& mExtensions;

  vk::PhysicalDevice            mPhysicalDevice;
  vk::PhysicalDeviceProperties2 mProperties2;
  std::string                   mDeviceName;

  vk::Instance mInstance;
};

template <class T>
void Device::setLabel(const ObjectNameInfo<T>& objectNameInfo) const
{
  if(!mEnableObjectLabeling)
  {
    return;
  }

  const std::string_view name = objectNameInfo.name.empty() ? "Unknown" : objectNameInfo.name;

  const auto nameInfo = vk::DebugUtilsObjectNameInfoEXT()
                            .setPObjectName(name.data())
                            .setObjectHandle(uint64_t(static_cast<T::CType>(objectNameInfo.handle)))
                            .setObjectType(objectNameInfo.handle.objectType);

  mDevice.setDebugUtilsObjectNameEXT(nameInfo);
}
}  // namespace ptvc::rhi
