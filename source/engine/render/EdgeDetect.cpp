#include "EdgeDetect.hpp"

#include <lib/lib.hpp>

EdgeDetect::EdgeDetect(SPtr<ptvc::rhi::VulkanContext> vulkanContext, SPtr<ptvc::rhi::Image> depthBuffer, glm::vec3 edgeColor, bool visualizeDepth)
    : mVulkanContext(std::move(vulkanContext))
    , mDepthBuffer(std::move(depthBuffer))
    , mEdgeColor(edgeColor, 1.0f)
    , mVisualizeDepth(visualizeDepth ? 1 : 0)
{
  createResources();
}

EdgeDetect::~EdgeDetect()
{
  if(mSampler)
  {
    mVulkanContext->getDevice()->getHandle().destroySampler(mSampler);
    mSampler = nullptr;
  }
}

void EdgeDetect::render(const ptvc::rhi::Frame& frame) noexcept
{
  // Transition the depth buffer from depth attachment to shader read-only
  {
    const auto depthToReadOnly = vk::ImageMemoryBarrier2()
                                     .setImage(mDepthBuffer->getHandle())
                                     .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1})
                                     .setOldLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                                     .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
                                     .setSrcStageMask(vk::PipelineStageFlagBits2::eLateFragmentTests)
                                     .setNewLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
                                     .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                                     .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader);

    frame.commandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(depthToReadOnly));
  }

  // Transition the swapchain image to color attachment for the contour pass
  const auto colorBarrier = mVulkanContext->getSwapchain()->getBarrier(
      frame.acquiredImageIndex, {.layout = vk::ImageLayout::eColorAttachmentOptimal,
                                 .accessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
                                 .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput});

  frame.commandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(colorBarrier));
  frame.commandBuffer.setScissor(0, mVulkanContext->getSwapchain()->getScissor());
  frame.commandBuffer.setViewport(0, mVulkanContext->getSwapchain()->getViewport());

  const auto colorAttachment = vk::RenderingAttachmentInfo()
                                   .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                   .setImageView(mVulkanContext->getSwapchain()->getImageView(frame.acquiredImageIndex))
                                   .setLoadOp(vk::AttachmentLoadOp::eLoad)
                                   .setStoreOp(vk::AttachmentStoreOp::eStore);

  const auto renderingInfo = vk::RenderingInfo()
                                 .setColorAttachments(colorAttachment)
                                 .setLayerCount(1)
                                 .setRenderArea(vk::Rect2D{{0, 0}, mVulkanContext->getSwapchain()->getExtent()});

  frame.commandBuffer.beginRendering(renderingInfo);

  mPipeline->bind(frame, frame.commandBuffer);

  const PushConstants pc{
      .edgeColor      = mEdgeColor,
      .threshold      = mThreshold,
      .visualizeDepth = mVisualizeDepth,
      .nearPlane      = mNearPlane,
      .farPlane       = mFarPlane,
  };
  mPipeline->pushConstant(&pc, frame.commandBuffer);

  // Fullscreen triangle, no vertex buffer required
  frame.commandBuffer.draw(3, 1, 0, 0);

  frame.commandBuffer.endRendering();

  // Transition the depth buffer back to depth attachment
  {
    const auto depthToAttachment =
        vk::ImageMemoryBarrier2()
            .setImage(mDepthBuffer->getHandle())
            .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1})
            .setOldLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
            .setSrcAccessMask(vk::AccessFlagBits2::eShaderRead)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
            .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests);

    frame.commandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(depthToAttachment));
  }
}

void EdgeDetect::createResources() noexcept
{
  using enum vk::ShaderStageFlagBits;

  {
    const auto samplerInfo = vk::SamplerCreateInfo()
                                 .setMagFilter(vk::Filter::eNearest)
                                 .setMinFilter(vk::Filter::eNearest)
                                 .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                                 .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                                 .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                                 .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                                 .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                                 .setCompareEnable(VK_FALSE)
                                 .setCompareOp(vk::CompareOp::eNever)
                                 .setMinLod(0.0f)
                                 .setMaxLod(0.0f)
                                 .setAnisotropyEnable(VK_FALSE);

    mSampler = mVulkanContext->getDevice()->getHandle().createSampler(samplerInfo);
  }

  mDescriptor = ptvc::rhi::Descriptor::create({
                                                  .bindings =
                                                      {
                                                          {0, vk::DescriptorType::eCombinedImageSampler, 1, eFragment},
                                                      },
                                                  .setCount = 1,
                                                  .label    = "EdgeDetect-Descriptor",
                                                  .device   = mVulkanContext->getDevice(),
                                              })
                    .value();

  {
    const auto imageInfo =
        vk::DescriptorImageInfo().setSampler(mSampler).setImageView(mDepthBuffer->getImageView()).setImageLayout(vk::ImageLayout::eDepthReadOnlyOptimal);

    const auto write = vk::WriteDescriptorSet()
                           .setImageInfo(imageInfo)
                           .setDstBinding(0)
                           .setDescriptorCount(1)
                           .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                           .setDstSet(mDescriptor->getSet(0));

    mVulkanContext->getDevice()->getHandle().updateDescriptorSets(write, {});
  }

  mPipeline = ptvc::rhi::GraphicsPipelineBuilder()
                  .addDescriptor(0, mDescriptor)
                  .addPushConstantRange({eFragment, 0, sizeof(PushConstants)})
                  .addShader({"assets/shaders/edgedetect.vert.glsl", eVertex})
                  .addShader({"assets/shaders/edgedetect.frag.glsl", eFragment})
                  .addAttachment(mVulkanContext->getSwapchain()->getFormat(),
                                 ptvc::rhi::detail::makeColorBlendAttachmentState(
                                     vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                         | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
                                     VK_TRUE, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
                                     vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd))
                  .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                    state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                    state.depthStencilState.setDepthTestEnable(false).setDepthWriteEnable(false);
                  })
                  .setName("EdgeDetectPipeline")
                  .create(mVulkanContext->getDevice());
}
