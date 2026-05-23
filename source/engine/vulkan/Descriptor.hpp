#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>
#include "Device.hpp"

namespace ptvc::rhi
{
    struct DescriptorCreateInfo
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings = {};
        std::uint32_t                               setCount = 1;
        std::string_view                            label    = "Unknown";
        SPtr<Device>                                device;
    };

    class Descriptor
    {
    public:
        [[nodiscard]] static Result<SPtr<Descriptor>> create(const DescriptorCreateInfo& createInfo) noexcept;

        ~Descriptor();

        [[nodiscard]] const vk::DescriptorSet& getSet(uint32_t i = 0) const noexcept;

        [[nodiscard]] const vk::DescriptorSetLayout& getLayout() const noexcept;

        [[nodiscard]] uint32_t getSetCount() const noexcept;

    private:
        Result<void> init(const DescriptorCreateInfo& createInfo) noexcept;

        explicit Descriptor(const DescriptorCreateInfo& createInfo);

        std::vector<vk::DescriptorSet>              mSets;
        std::vector<vk::DescriptorSetLayoutBinding> mLayoutBindings;
        vk::DescriptorSetLayout                     mLayout;
        vk::DescriptorPool                          mDescriptorPool;
        std::string                                 mLabel;
        uint32_t                                    mSetCount;
        SPtr<Device>                                mDevice;
    };
}
