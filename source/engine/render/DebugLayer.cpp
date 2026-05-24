#include "DebugLayer.hpp"

#include <imgui.h>
#include <scene/Terrain.hpp>

namespace ptvc {
namespace detail {
[[nodiscard]] static std::string toString(const DebugRenderMode renderMode) noexcept
{
  using enum DebugRenderMode;
  switch(renderMode)
  {
    case eNone:
      return "ObjIndex";
    case eNormal:
      return "Normal";
    case eUV:
      return "UV";
    case eWireframe:
      return "Wireframe";
    case eGame:
      return "Game";
  }
  return "Unknown";
}
}  // namespace detail

DebugLayer::DebugLayer()
{
  const auto* app = Application::getApplication();
  mVulkanContext  = app->getVulkanContext();
  mScene          = app->getScene();

  createDebugPipeline();

  mEngine = std::mt19937(std::random_device{}());
  std::uniform_real_distribution c(0.0f, 1.0f);
  for(auto& color : mObjectColors)
  {
    color = glm::vec4(c(mEngine), c(mEngine), c(mEngine), 1.0f);
  }
}

void DebugLayer::onEvent(const SDL_Event& event) noexcept
{
  // Toggle debug layer
  // =============================
  if(event.type == SDL_EVENT_KEY_DOWN)
  {
    const SDL_KeyboardEvent& keyboardEvent = event.key;
    if(keyboardEvent.scancode == mConfig.toggleKey)
    {
      mEnabled = !mEnabled;
      spdlog::debug("Toggled debug renderer: {}", STYLE_BOOL(mEnabled, "ON", "OFF"));
    }
  }
}

void DebugLayer::onRender(const rhi::Frame& frame) noexcept
{
  if(!mEnabled)
  {
    return;
  }

  std::array<vk::ImageMemoryBarrier2, 2> barriers;

  barriers[0] = mVulkanContext->getSwapchain()->getBarrier(
      frame.acquiredImageIndex, {
                                    .layout = vk::ImageLayout::eColorAttachmentOptimal,
                                    .accessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
                                    .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                });

  barriers[1] = vk::ImageMemoryBarrier2()
                    .setImage(mDepthBuffer->getHandle())
                    .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1})
                    .setOldLayout(mIsFirstRender ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthAttachmentOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                    .setSrcStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
                    .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                    .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
                    .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests);

  const auto dependencyInfo = vk::DependencyInfo().setImageMemoryBarriers(barriers);

  frame.commandBuffer.pipelineBarrier2(dependencyInfo);

  frame.commandBuffer.setScissor(0, mVulkanContext->getSwapchain()->getScissor());
  frame.commandBuffer.setViewport(0, mVulkanContext->getSwapchain()->getViewport());

  const auto colorAttachment = vk::RenderingAttachmentInfo()
                                   .setClearValue(vk::ClearValue().setColor({0.0f, 0.0f, 0.0f, 1.0f}))
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
  for(auto&& [objIndex, obj] : enumerate(mScene->getGameObjects()))
  {
    const auto pushConstant0 = obj->getGPUData();
    const auto pushConstant1 = DebugPipeline_PCS{
        .model      = pushConstant0.model,
        .solidColor = mObjectColors[objIndex % mObjectColors.size()],
        .renderMode = std::to_underlying(mConfig.mode),
        .objIndex   = static_cast<int32_t>(objIndex),
    };

    mPipeline->bind(frame, frame.commandBuffer);
    mPipeline->pushConstant(&pushConstant1, frame.commandBuffer);

    obj->getGeometry()->draw(frame.commandBuffer);
  }

  // Render terrain with debug mode pushed to its shader
  if(const auto* terrain = mScene->getTerrain())
    terrain->onRender(frame, mConfig.mode);

  frame.commandBuffer.endRendering();

  if(mIsFirstRender)
  {
    mIsFirstRender = false;
  }
}

void DebugLayer::onDrawUI() noexcept
{
  if(!mConfig.enableUI || !mEnabled)
  {
    return;
  }

  ImGui::Begin("Debug Render Options");

  ImGui::Text("Hint: Toggle with [K]");

  const auto modeStr = fmt::format("Current Mode: {}", detail::toString(mConfig.mode));
  ImGui::SeparatorText(modeStr.c_str());

  if(ImGui::SmallButton("ObjIndex"))
  {
    mConfig.mode = DebugRenderMode::eNone;
  }
  ImGui::SameLine();
  ImGui::Text("Random colored objects");

  if(ImGui::SmallButton("Normals"))
  {
    mConfig.mode = DebugRenderMode::eNormal;
  }
  ImGui::SameLine();
  ImGui::Text("Visualize vertex normals");

  if(ImGui::SmallButton("UV"))
  {
    mConfig.mode = DebugRenderMode::eUV;
  }
  ImGui::SameLine();
  ImGui::Text("Visualize vertex UVs");

  ImGui::End();
}

bool DebugLayer::isEnabled() const noexcept
{
  return mEnabled;
}

void DebugLayer::createDebugPipeline() noexcept
{
  using enum vk::ShaderStageFlagBits;
  mDepthBuffer = rhi::Image::create({
                                        .extent     = mVulkanContext->getSwapchain()->getExtent(),
                                        .usageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                        .format     = vk::Format::eD32Sfloat,
                                        .mipmapping = false,
                                        .samples    = vk::SampleCountFlagBits::e1,
                                        .label      = "BasicPipeline_DepthBuffer",
                                        .device     = mVulkanContext->getDevice(),
                                    })
                     .value();

  mPipeline = rhi::GraphicsPipelineBuilder()
                  .addDescriptor(0, mScene->getDescriptor())
                  .addPushConstantRange({eVertex | eFragment, 0, sizeof(DebugPipeline_PCS)})
                  .addVertexType<Vertex>()
                  .addShader({"assets/shaders/debug.vert.glsl", eVertex})
                  .addShader({"assets/shaders/debug.frag.glsl", eFragment})
                  .addAttachment(vk::Format::eB8G8R8A8Unorm)
                  .setDepthFormat(vk::Format::eD32Sfloat)
                  .setName("DebugPipeline")
                  .create(mVulkanContext->getDevice());
}
}  // namespace ptvc
