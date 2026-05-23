#include "Pipeline.hpp"

namespace ptvc::rhi {
void Pipeline::bind(const Frame& frame, const vk::CommandBuffer& commandBuffer) noexcept
{
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
  for(auto&& [setIndex, descriptor] : mDescriptors)
  {
    auto set = descriptor->getSet(frame.currentFrameIndex % descriptor->getSetCount());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mPipelineLayout, setIndex, 1, &set, 0, nullptr);
  }
}

void Pipeline::pushConstant(const void* pData, const vk::CommandBuffer& commandBuffer, const uint32_t pcRangeIndex) const noexcept
{
  if(pcRangeIndex >= mPushConstantRanges.size())
  {
    exitWithError("Invalid PushConstantRange index: {}, the Pipeline ({}) was created with {} range(s).", pcRangeIndex,
                  "TODO_LABEL" /* mLabel */, mPushConstantRanges.size());
  }
  const auto& pcr = mPushConstantRanges[pcRangeIndex];
  commandBuffer.pushConstants(mPipelineLayout, pcr.stageFlags, pcr.offset, pcr.size, pData);
}

const vk::Pipeline& Pipeline::getHandle() const noexcept
{
  return mPipeline;
}

const vk::PipelineLayout& Pipeline::getLayout() const noexcept
{
  return mPipelineLayout;
}
}  // namespace ptvc::rhi
