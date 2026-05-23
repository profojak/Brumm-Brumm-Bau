#include "Descriptor.hpp"

namespace ptvc::rhi {
Result<SPtr<Descriptor>> Descriptor::create(const DescriptorCreateInfo& createInfo) noexcept
{
  if(!createInfo.device)
  {
    exitWithError("Failed to create Buffer: Device is null");
  }

  auto descriptor = SPtr<Descriptor>(std::move(new Descriptor(createInfo)));
  if(const auto result = descriptor->init(createInfo); !result.has_value())
  {
    return std::unexpected(result.error());
  }
  return descriptor;
}

Descriptor::~Descriptor()
{
  const vk::Device device = mDevice->getHandle();
  device.waitIdle();

  if(const auto result = device.freeDescriptorSets(mDescriptorPool, mSetCount, mSets.data()); result != vk::Result::eSuccess)
  {
    spdlog::warn("Failed to free DescriptorSets of {}", mLabel);
  }

  device.destroyDescriptorPool(mDescriptorPool);
  device.destroyDescriptorSetLayout(mLayout);
}

const vk::DescriptorSet& Descriptor::getSet(const uint32_t i) const noexcept
{
  assert(i < mSets.size());
  return mSets[i];
}

const vk::DescriptorSetLayout& Descriptor::getLayout() const noexcept
{
  return mLayout;
}

uint32_t Descriptor::getSetCount() const noexcept
{
  return mSets.size();
}

Result<void> Descriptor::init(const DescriptorCreateInfo& createInfo) noexcept
{
  /* DescriptorPool */ {
    const auto poolSizes =
        mLayoutBindings | std::views::transform([&](const auto& b) {
          return vk::DescriptorPoolSize().setDescriptorCount(b.descriptorCount * mSetCount).setType(b.descriptorType);
        })
        | std::ranges::to<std::vector>();
    const auto poolCreateInfo = vk::DescriptorPoolCreateInfo()
                                    .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                                    .setMaxSets(mSetCount)
                                    .setPoolSizes(poolSizes);
    VK_CATCH(mDescriptorPool = mDevice->getHandle().createDescriptorPool(poolCreateInfo));

    mDevice->setLabel<vk::DescriptorPool>({
        .name   = std::format("{}-Pool", mLabel),
        .handle = mDescriptorPool,
    });
  }

  /* DescriptorSetLayout */ {
    const auto layoutCreateInfo = vk::DescriptorSetLayoutCreateInfo().setBindings(mLayoutBindings);
    VK_CATCH(mLayout = mDevice->getHandle().createDescriptorSetLayout(layoutCreateInfo));

    mDevice->setLabel<vk::DescriptorSetLayout>({
        .name   = std::format("{}-Layout", mLabel),
        .handle = mLayout,
    });
  }

  /* DescriptorSets */ {
    mSets.resize(mSetCount);
    const std::vector layouts(mSetCount, mLayout);
    const auto        allocateInfo = vk::DescriptorSetAllocateInfo()
                                  .setDescriptorPool(mDescriptorPool)
                                  .setDescriptorSetCount(mSetCount)
                                  .setPSetLayouts(layouts.data());
    VK_RESULT(mDevice->getHandle().allocateDescriptorSets(&allocateInfo, mSets.data()));

    for(auto&& [i, set] : enumerate(mSets))
    {
      mDevice->setLabel<vk::DescriptorSet>({
          .name   = std::format("{}-Set#{}", mLabel, i),
          .handle = set,
      });
    }
  }

  return {};
}

Descriptor::Descriptor(const DescriptorCreateInfo& createInfo)
    : mLayoutBindings(createInfo.bindings)
    , mLabel(createInfo.label)
    , mSetCount(createInfo.setCount)
    , mDevice(createInfo.device)
{
}
}  // namespace ptvc::rhi
