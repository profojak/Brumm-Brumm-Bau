#include "GameLayer.hpp"

#include <core/Application.hpp>
#include <render/DebugLayer.hpp>
#include <vulkan/render/GraphicsPipeline.hpp>

GameLayer::GameLayer()
{
  const auto* app = ptvc::Application::getApplication();
  mVulkanContext  = app->getVulkanContext();
  mScene          = app->getScene();

  createDepthBuffer();

  mTerrain = makeUnique<ptvc::Terrain>(mVulkanContext, mScene->getDescriptor());
  mScene->setTerrain(mTerrain.get());
}

GameLayer::~GameLayer() {}

void GameLayer::onEvent(const SDL_Event& event) noexcept
{
  // Forward events to the scene
  mScene->onEvent(event);
}

void GameLayer::onUpdate(const float deltaTime) noexcept {}

void GameLayer::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  const auto cameraData = mScene->getCamera().getCameraData();
  mTerrain->updateTessellationData(cameraData, 0.3f);

  // Transition swapchain image
  const auto colorBarrier = mVulkanContext->getSwapchain()->getBarrier(
      frame.acquiredImageIndex, {.layout = vk::ImageLayout::eColorAttachmentOptimal,
                                 .accessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
                                 .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput});

  // Transition depth buffer
  const auto depthBarrier =
      vk::ImageMemoryBarrier2()
          .setImage(mDepthBuffer->getHandle())
          .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1})
          .setOldLayout(mFirstRender ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthAttachmentOptimal)
          .setSrcAccessMask(vk::AccessFlagBits2::eNone)
          .setSrcStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
          .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
          .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
          .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests);

  std::array barriers = {colorBarrier, depthBarrier};
  frame.commandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));

  frame.commandBuffer.setScissor(0, mVulkanContext->getSwapchain()->getScissor());
  frame.commandBuffer.setViewport(0, mVulkanContext->getSwapchain()->getViewport());

  // Begin rendering
  const auto colorAttachment = vk::RenderingAttachmentInfo()
                                   .setClearValue(vk::ClearValue().setColor({0.1f, 0.15f, 0.2f, 1.0f}))
                                   .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                   .setImageView(mVulkanContext->getSwapchain()->getImageView(frame.acquiredImageIndex))
                                   .setLoadOp(vk::AttachmentLoadOp::eClear)
                                   .setStoreOp(vk::AttachmentStoreOp::eStore);

  const auto depthAttachment = vk::RenderingAttachmentInfo()
                                   .setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}))
                                   .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                                   .setImageView(mDepthBuffer->getImageView())
                                   .setLoadOp(vk::AttachmentLoadOp::eClear)
                                   .setStoreOp(vk::AttachmentStoreOp::eStore);

  const auto renderingInfo = vk::RenderingInfo()
                                 .setColorAttachments(colorAttachment)
                                 .setPDepthAttachment(&depthAttachment)
                                 .setLayerCount(1)
                                 .setRenderArea(vk::Rect2D{{0, 0}, mVulkanContext->getSwapchain()->getExtent()});

  frame.commandBuffer.beginRendering(renderingInfo);

  // Render the tessellated terrain
  mTerrain->onRender(frame, ptvc::DebugRenderMode::eNone);

  frame.commandBuffer.endRendering();

  if(mFirstRender)
    mFirstRender = false;
}

void GameLayer::createDepthBuffer() noexcept
{
  const auto result = ptvc::rhi::Image::create({
      .extent     = mVulkanContext->getSwapchain()->getExtent(),
      .usageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .format     = vk::Format::eD32Sfloat,
      .mipmapping = false,
      .samples    = vk::SampleCountFlagBits::e1,
      .label      = "GameLayer-DepthBuffer",
      .device     = mVulkanContext->getDevice(),
  });
  exitOnError(result);
  mDepthBuffer = std::move(result.value());
}
