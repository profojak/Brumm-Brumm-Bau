#pragma once

#include <vector>

#include "Descriptor.hpp"
#include "Device.hpp"
#include "Pipeline.hpp"
#include "Shaders.hpp"
#include "VertexType.hpp"

namespace ptvc::rhi {
namespace detail {
vk::PipelineInputAssemblyStateCreateInfo makeInputAssemblyState() noexcept;

vk::PipelineRasterizationStateCreateInfo makeRasterizationState() noexcept;

vk::PipelineMultisampleStateCreateInfo makeMultisampleState() noexcept;

vk::PipelineDepthStencilStateCreateInfo makeDepthStencilState() noexcept;

vk::PipelineViewportStateCreateInfo makeViewportState() noexcept;

vk::PipelineDynamicStateCreateInfo makeDynamicState() noexcept;

vk::PipelineColorBlendStateCreateInfo makeColorBlendState() noexcept;

vk::PipelineVertexInputStateCreateInfo makeVertexInputState() noexcept;

using Clr = vk::ColorComponentFlagBits;

static vk::PipelineColorBlendAttachmentState makeColorBlendAttachmentState(
    const vk::ColorComponentFlags colorWriteMask      = Clr::eR | Clr::eG | Clr::eB | Clr::eA,
    const vk::Bool32              blendEnable         = false,
    const vk::BlendFactor         srcColorBlendFactor = vk::BlendFactor::eOne,
    const vk::BlendFactor         dstColorBlendFactor = vk::BlendFactor::eZero,
    const vk::BlendOp             colorBlendOp        = vk::BlendOp::eAdd,
    const vk::BlendFactor         srcAlphaBlendFactor = vk::BlendFactor::eOne,
    const vk::BlendFactor         dstAlphaBlendFactor = vk::BlendFactor::eZero,
    const vk::BlendOp             alphaBlendOp        = vk::BlendOp::eAdd)
{
  return vk::PipelineColorBlendAttachmentState()
      .setColorWriteMask(colorWriteMask)
      .setBlendEnable(blendEnable)
      .setSrcColorBlendFactor(srcColorBlendFactor)
      .setDstColorBlendFactor(dstColorBlendFactor)
      .setColorBlendOp(colorBlendOp)
      .setSrcAlphaBlendFactor(srcAlphaBlendFactor)
      .setDstAlphaBlendFactor(dstAlphaBlendFactor)
      .setAlphaBlendOp(alphaBlendOp);
}
}  // namespace detail

/**
* Struct containing all graphics pipeline state related data
*/
struct GraphicsPipelineState
{
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState = detail::makeInputAssemblyState();
  vk::PipelineRasterizationStateCreateInfo rasterizationState = detail::makeRasterizationState();
  vk::PipelineMultisampleStateCreateInfo   multisampleState   = detail::makeMultisampleState();
  vk::PipelineDepthStencilStateCreateInfo  depthStencilState  = detail::makeDepthStencilState();
  vk::PipelineViewportStateCreateInfo      viewportState      = detail::makeViewportState();
  vk::PipelineDynamicStateCreateInfo       dynamicState       = detail::makeDynamicState();
  vk::PipelineColorBlendStateCreateInfo    colorBlendState    = detail::makeColorBlendState();
  vk::PipelineVertexInputStateCreateInfo   vertexInputState   = detail::makeVertexInputState();
  vk::PipelineTessellationStateCreateInfo tessellationState = vk::PipelineTessellationStateCreateInfo().setPatchControlPoints(0);

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eScissor, vk::DynamicState::eViewport};
  std::vector<vk::Format>       attachmentFormats;
  std::vector<vk::PipelineColorBlendAttachmentState> attachmentStates;
  vk::Format                                         depthFormat   = vk::Format::eUndefined;
  vk::Format                                         stencilFormat = vk::Format::eUndefined;
};

class GraphicsPipelineBuilder
{
public:
  template <VertexType T>
  GraphicsPipelineBuilder& addVertexType(uint32_t binding = 0) noexcept
  {
    const auto nextLocation = static_cast<uint32_t>(mVertexInput._lastLocation + 1);

    mVertexInput.attributeDescriptions.append_range(T::getAttributes(nextLocation, binding));
    mVertexInput.bindingDescriptions.push_back(T::getBinding(binding));
    return *this;
  }

  GraphicsPipelineBuilder& addAttachment(vk::Format format,
                                         const vk::PipelineColorBlendAttachmentState& state = detail::makeColorBlendAttachmentState()) noexcept;

  GraphicsPipelineBuilder& setDepthFormat(vk::Format format) noexcept;

  GraphicsPipelineBuilder& configure(const std::function<void(GraphicsPipelineState&)>& fn) noexcept;

  GraphicsPipelineBuilder& addPushConstantRange(const vk::PushConstantRange& value) noexcept;

  GraphicsPipelineBuilder& addDescriptorSetLayout(const vk::DescriptorSetLayout& value) noexcept;

  GraphicsPipelineBuilder& addDescriptor(uint32_t setIndex, const SPtr<Descriptor>& descriptor) noexcept;

  GraphicsPipelineBuilder& addShader(const ShaderInfo& shaderInfo) noexcept;

  GraphicsPipelineBuilder& setName(std::string_view name) noexcept;

  UPtr<Pipeline> create(const SPtr<Device>& device) noexcept;

private:
  std::string                                    mName;
  std::vector<ShaderInfo>                        mShaders;
  std::vector<vk::PushConstantRange>             mPushConstantRanges;
  std::vector<vk::DescriptorSetLayout>           mDescriptorSetLayouts;
  std::unordered_map<uint32_t, SPtr<Descriptor>> mDescriptors;
  GraphicsPipelineState                          mState = {};

  struct GraphicsPipelineVertexInput
  {
    int32_t                                          _lastLocation = -1;
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    std::vector<vk::VertexInputBindingDescription>   bindingDescriptions;
  } mVertexInput;
};
}  // namespace ptvc::rhi
