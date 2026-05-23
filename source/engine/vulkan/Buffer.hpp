#pragma once

#include <string>
#include <string_view>
#include <vk_mem_alloc.h>
#include <lib/lib.hpp>

#include "VulkanCore.hpp"

namespace ptvc::rhi
{
    class Device;

    struct BufferCreateInfo
    {
        vk::DeviceSize          size        = 0;
        vk::BufferUsageFlags    usageFlags  = {};
        bool                    hostVisible = false;
        std::string_view        label       = "Unknown Buffer";
        SPtr<Device>            device      = nullptr;
    };

    /**
     * Wrapper class for a Vulkan Buffer
     * - All buffers are marked for usages: TransferSrc, TransferDst, ShaderDeviceAddress
     */
    class Buffer
    {
    public:
        DISABLE_COPY(Buffer);

        [[nodiscard]] static Result<SPtr<Buffer>> create(const BufferCreateInfo& createInfo) noexcept;

        ~Buffer();

        /**
         * Map the memory of a host visible Buffer.
         * @param ptr Host side pointer
         */
        void map(void* ptr) const noexcept;

        /**
         * Unmap the memory of a host visible Buffer.
         */
        void unmap() const noexcept;

        /**
         * Set the data of a host visible Buffer.
         * @param pData Pointer to the host-side data
         * @param size Size of the host-side data
         * @param offset Offset within the Buffer memory (default = 0)
         */
        void setData(const void* pData, vk::DeviceSize size, vk::DeviceSize offset = 0) const noexcept;

        /**
         * Read data from a host visible Buffer into a host-side pointer.
         * @param pData Host-side pointer
         * @param size Size of the data to read back
         * @param offset Offset within the Buffer memory (default = 0)
         */
        void readBack(void* pData, vk::DeviceSize size, vk::DeviceSize offset = 0) const noexcept;

        [[nodiscard]] const vk::Buffer& getHandle()  const noexcept;

        [[nodiscard]] vk::DeviceSize getSize() const noexcept;

        [[nodiscard]] vk::DeviceAddress getAddress() const noexcept;

    private:
        Result<void> createBuffer(const BufferCreateInfo& createInfo) noexcept;

        explicit Buffer(const BufferCreateInfo& createInfo);

        bool                mHostVisible = false;
        vk::DeviceSize      mSize        = 0;
        vk::DeviceAddress   mAddress     = 0;
        std::string         mLabel;

        vk::Buffer          mBuffer;
        VmaAllocation       mAllocation {};
        VmaAllocationInfo   mAllocationInfo {};

        SPtr<Device>        mDevice;
    };
}
