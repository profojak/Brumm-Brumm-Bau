#include "Buffer.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include "Device.hpp"

// Host visible Buffer guard
#define check_is_host_visible(msg)                                                                                     \
  if(!mHostVisible)                                                                                                    \
  {                                                                                                                    \
    exitWithError("{} (not host visible)", msg);                                                                       \
  }

// Buffer bounds check
#define check_bounds(msg, testSize, realSize)                                                                          \
  if(testSize > realSize)                                                                                              \
  {                                                                                                                    \
    exitWithError("{} (Size {} out of bounds for Buffer of size {})", msg, testSize, realSize);                        \
  }

// Error messages
constexpr auto msg_setDataFailed  = "Failed to set Buffer data";
constexpr auto msg_readBackFailed = "Failed to read Buffer data";

namespace ptvc::rhi {
Result<SPtr<Buffer>> Buffer::create(const BufferCreateInfo& createInfo) noexcept
{
  if(!createInfo.device)
  {
    exitWithError("Failed to create Buffer: Device is null");
  }

  if(createInfo.size == 0)
  {
    return std::unexpected(std::format("Failed to create Buffer: Invalid size {}", createInfo.size));
  }

  auto buffer = SPtr<Buffer>(std::move(new Buffer(createInfo)));
  if(const auto result = buffer->createBuffer(createInfo); !result.has_value())
  {
    return std::unexpected(result.error());
  }
  return buffer;
}

Buffer::~Buffer()
{
  vmaDestroyBuffer(mDevice->getAllocator(), mBuffer, mAllocation);
}

void Buffer::map(void* ptr) const noexcept
{
  check_is_host_visible("Failed to map Buffer memory");
  vmaMapMemory(mDevice->getAllocator(), mAllocation, &ptr);
}

void Buffer::unmap() const noexcept
{
  check_is_host_visible("Failed to unmap Buffer memory");
  vmaUnmapMemory(mDevice->getAllocator(), mAllocation);
}

void Buffer::setData(const void* pData, const vk::DeviceSize size, const vk::DeviceSize offset) const noexcept
{
  check_is_host_visible(msg_setDataFailed);
  check_bounds(msg_setDataFailed, size, mSize);

  if(const auto result = vmaCopyMemoryToAllocation(mDevice->getAllocator(), pData, mAllocation, offset, size); result != VK_SUCCESS)
  {
    exitWithError("CopyMemoryToAllocation failed: {}", string_VkResult(result));
  }
}

void Buffer::readBack(void* pData, const vk::DeviceSize size, const vk::DeviceSize offset) const noexcept
{
  check_is_host_visible(msg_readBackFailed);
  check_bounds(msg_readBackFailed, size, mSize);

  if(const auto result = vmaCopyAllocationToMemory(mDevice->getAllocator(), mAllocation, offset, pData, size); result != VK_SUCCESS)
  {
    exitWithError("CopyAllocationToMemory failed: {}", string_VkResult(result));
  }
}

const vk::Buffer& Buffer::getHandle() const noexcept
{
  return mBuffer;
}

vk::DeviceSize Buffer::getSize() const noexcept
{
  return mSize;
}

vk::DeviceAddress Buffer::getAddress() const noexcept
{
  return mAddress;
}

Result<void> Buffer::createBuffer(const BufferCreateInfo& createInfo) noexcept
{
  using enum vk::BufferUsageFlagBits;

  auto bufferInfo =
      vk::BufferCreateInfo().setSize(createInfo.size).setUsage(createInfo.usageFlags | eTransferSrc | eTransferDst | eShaderDeviceAddress);

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage                   = VMA_MEMORY_USAGE_AUTO;

  if(createInfo.hostVisible)
  {
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  const auto result = vmaCreateBuffer(mDevice->getAllocator(), reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo),
                                      &allocInfo, reinterpret_cast<VkBuffer*>(&mBuffer), &mAllocation, &mAllocationInfo);

  if(result != VK_SUCCESS)
  {
    return std::unexpected(string_VkResult(result));
  }

  mDevice->setLabel<vk::Buffer>({
      .name   = mLabel,
      .handle = mBuffer,
  });

  const auto addressInfo = vk::BufferDeviceAddressInfo().setBuffer(mBuffer);
  mAddress               = mDevice->getHandle().getBufferAddress(&addressInfo);

  return {};
}

Buffer::Buffer(const BufferCreateInfo& createInfo)
    : mHostVisible(createInfo.hostVisible)
    , mSize(createInfo.size)
    , mLabel(createInfo.label)
    , mDevice(createInfo.device)
{
}
}  // namespace ptvc::rhi

#undef check_is_host_visible
#undef check_bounds
