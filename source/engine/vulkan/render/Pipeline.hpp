#pragma once

#include "Descriptor.hpp"
#include "Device.hpp"
#include "../VulkanCore.hpp"

namespace ptvc::rhi
{
    class Pipeline
    {
    public:
        Pipeline() = default;

        void bind(const Frame& frame, const vk::CommandBuffer& commandBuffer) noexcept;

        /**
         * Upload push constant data to the GPU
         * @param pData Pointer to the data
         * @param commandBuffer Actively recording CommandBuffer
         * @param pcRangeIndex Index for which push constant range to upload to (wrt. the order specified at pipeline creation)
         */
        void pushConstant(const void* pData, const vk::CommandBuffer& commandBuffer, const uint32_t pcRangeIndex = 0) const noexcept;

        [[nodiscard]] const vk::Pipeline& getHandle() const noexcept;

        [[nodiscard]] const vk::PipelineLayout& getLayout() const noexcept;

    private:
        friend class GraphicsPipelineBuilder;

        SPtr<Device>                                    mDevice;

        vk::Pipeline                                    mPipeline;
        vk::PipelineLayout                              mPipelineLayout;
        std::vector<vk::PushConstantRange>              mPushConstantRanges;
        std::unordered_map<uint32_t, SPtr<Descriptor>>  mDescriptors;
        std::string                                     mName;
    };
}
