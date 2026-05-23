#include "GraphicsPipeline.hpp"

#include <ranges>

namespace ptvc::rhi
{
    namespace detail
    {
        vk::PipelineInputAssemblyStateCreateInfo makeInputAssemblyState() noexcept
        {
            return vk::PipelineInputAssemblyStateCreateInfo()
                .setTopology(vk::PrimitiveTopology::eTriangleList)
                .setPrimitiveRestartEnable(false)
                .setFlags({})
                .setPNext(nullptr);
        }

        vk::PipelineRasterizationStateCreateInfo makeRasterizationState() noexcept
        {
            return vk::PipelineRasterizationStateCreateInfo()
                .setPolygonMode(vk::PolygonMode::eFill)
                .setCullMode(vk::CullModeFlagBits::eBack)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setDepthClampEnable(false)
                .setDepthBiasEnable(false)
                .setDepthBiasClamp(0.0f)
                .setDepthBiasSlopeFactor(0.0f)
                .setLineWidth(1.0f)
                .setRasterizerDiscardEnable(false)
                .setPNext(nullptr);
        }

        vk::PipelineMultisampleStateCreateInfo makeMultisampleState() noexcept
        {
            return vk::PipelineMultisampleStateCreateInfo()
                .setRasterizationSamples(vk::SampleCountFlagBits::e1)
                .setSampleShadingEnable(false)
                .setPSampleMask(nullptr)
                .setAlphaToCoverageEnable(false)
                .setAlphaToOneEnable(false)
                .setPNext(nullptr);
        }

        vk::PipelineDepthStencilStateCreateInfo makeDepthStencilState() noexcept
        {
            return vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);
        }

        vk::PipelineViewportStateCreateInfo makeViewportState() noexcept
        {
            return vk::PipelineViewportStateCreateInfo()
                .setViewportCount(1)
                .setPViewports(nullptr)
                .setScissorCount(1)
                .setPScissors(nullptr)
                .setPNext(nullptr);
        }

        vk::PipelineDynamicStateCreateInfo makeDynamicState() noexcept
        {
            return vk::PipelineDynamicStateCreateInfo()
                .setDynamicStateCount(0)
                .setPDynamicStates(nullptr)
                .setPNext(nullptr);
        }

        vk::PipelineColorBlendStateCreateInfo makeColorBlendState() noexcept
        {
            return vk::PipelineColorBlendStateCreateInfo()
                .setLogicOp(vk::LogicOp::eClear)
                .setLogicOpEnable(false)
                .setAttachmentCount(0)
                .setPAttachments(nullptr)
                .setBlendConstants({0.0f, 0.0f, 0.0f, 0.0f})
                .setPNext(nullptr);
        }

        vk::PipelineVertexInputStateCreateInfo makeVertexInputState() noexcept
        {
            return vk::PipelineVertexInputStateCreateInfo()
                .setVertexAttributeDescriptionCount(0)
                .setPVertexAttributeDescriptions(nullptr)
                .setVertexBindingDescriptionCount(0)
                .setPVertexBindingDescriptions(nullptr)
                .setPNext(nullptr);
        }
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::addAttachment(const vk::Format format, const vk::PipelineColorBlendAttachmentState& state) noexcept
    {
        mState.attachmentFormats.push_back(format);
        mState.attachmentStates.push_back(state);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDepthFormat(const vk::Format format) noexcept
    {
        mState.depthFormat = format;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::configure(const std::function<void(GraphicsPipelineState&)>& fn) noexcept
    {
        fn(mState);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::addPushConstantRange(const vk::PushConstantRange& value) noexcept
    {
        mPushConstantRanges.push_back(value);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::addDescriptorSetLayout(const vk::DescriptorSetLayout& value) noexcept
    {
        mDescriptorSetLayouts.push_back(value);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::addDescriptor(const uint32_t setIndex, const SPtr<Descriptor>& descriptor) noexcept
    {
        mDescriptors.insert_or_assign(setIndex, descriptor);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::addShader(const ShaderInfo& shaderInfo) noexcept
    {
        mShaders.push_back(shaderInfo);
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::setName(const std::string_view name) noexcept
    {
        mName = name;
        return *this;
    }

    UPtr<Pipeline> GraphicsPipelineBuilder::create(const SPtr<Device>& device) noexcept
    {
        auto result = makeUnique<Pipeline>();

        auto keys = mDescriptors | std::views::keys | std::ranges::to<std::vector>();
        std::ranges::sort(keys);
        for (const auto setIndex : keys)
        {
            mDescriptorSetLayouts.push_back(mDescriptors[setIndex]->getLayout());
        }

        mState.colorBlendState.setAttachments(mState.attachmentStates);
        mState.vertexInputState.setVertexAttributeDescriptions(mVertexInput.attributeDescriptions);
        mState.vertexInputState.setVertexBindingDescriptions(mVertexInput.bindingDescriptions);
        mState.dynamicState.setDynamicStates(mState.dynamicStates);

        const auto layoutCreateInfo = vk::PipelineLayoutCreateInfo()
            .setSetLayouts(mDescriptorSetLayouts)
            .setPushConstantRanges(mPushConstantRanges);

        result->mDevice = device;
        result->mPipelineLayout = device->getHandle().createPipelineLayout(layoutCreateInfo);
        device->setLabel<vk::PipelineLayout>({
            .name   = std::format("{}-Layout", mName),
            .handle = result->mPipelineLayout,
        });

        const auto shaders = Shaders::createShaderModules(mShaders, device.get());
        const auto shaderStageInfos = shaders
            | std::views::transform([](const auto& x){ return x.shaderStageInfo; })
            | std::ranges::to<std::vector>();

        const auto renderingInfo = vk::PipelineRenderingCreateInfo()
            .setColorAttachmentFormats(mState.attachmentFormats)
            .setDepthAttachmentFormat(mState.depthFormat)
            .setStencilAttachmentFormat(mState.stencilFormat);

        const auto graphicsPipelineCreateInfo = vk::GraphicsPipelineCreateInfo()
            .setPInputAssemblyState(&mState.inputAssemblyState)
            .setPRasterizationState(&mState.rasterizationState)
            .setPMultisampleState(&mState.multisampleState)
            .setPDepthStencilState(&mState.depthStencilState)
            .setPViewportState(&mState.viewportState)
            .setPDynamicState(&mState.dynamicState)
            .setPColorBlendState(&mState.colorBlendState)
            .setPVertexInputState(&mState.vertexInputState)
            .setStageCount(shaderStageInfos.size())
            .setPStages(shaderStageInfos.data())
            .setLayout(result->mPipelineLayout)
            .setRenderPass(nullptr)
            .setPNext(&renderingInfo);

        result->mPipeline = device->getHandle().createGraphicsPipeline(nullptr, graphicsPipelineCreateInfo).value;
        result->mDescriptors = mDescriptors;
        result->mPushConstantRanges = mPushConstantRanges;
        result->mName = mName;
        device->setLabel<vk::Pipeline>({
            .name   = mName,
            .handle = result->mPipeline,
        });

        return result;
    }
}
