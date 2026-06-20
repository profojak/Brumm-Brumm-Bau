#include "DebugLayer.hpp"

#include <imgui.h>
#include <scene/Terrain.hpp>
#include <scene/OrbitCamera.hpp>
#include <scene/FreeCamera.hpp>

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
    case eDepthEdges:
      return "Depth";
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
  createWireframePipeline();

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
    if(keyboardEvent.scancode == SDL_SCANCODE_O)
    {
      mShowOptions = !mShowOptions;
      spdlog::debug("Toggled options window: {}", STYLE_BOOL(mShowOptions, "ON", "OFF"));
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
  const auto renderingInfo   = vk::RenderingInfo()
                                   .setColorAttachments(colorAttachment)
                                   .setPDepthAttachment(&depthAttachment)
                                   .setLayerCount(1)
                                   .setRenderArea(vk::Rect2D{{0, 0}, mVulkanContext->getSwapchain()->getExtent()});

  frame.commandBuffer.beginRendering(renderingInfo);
  std::vector<GameObject::DebugMesh> debugMeshes;
  for(auto&& [objIndex, obj] : enumerate(mScene->getGameObjects()))
  {
    debugMeshes.clear();
    obj->collectDebugMeshes(debugMeshes);
    if(debugMeshes.empty())
      continue;

    const auto solidColor = mObjectColors[objIndex % mObjectColors.size()];
    const auto renderMode = std::to_underlying(mConfig.mode);
    const auto objIdx     = static_cast<int32_t>(objIndex);

    auto& pipeline = (mConfig.mode == DebugRenderMode::eWireframe) ? mWireframePipeline : mPipeline;
    pipeline->bind(frame, frame.commandBuffer);

    for(const auto& mesh : debugMeshes)
    {
      if(mesh.geometry == nullptr)
        continue;

      const auto pushConstant1 = DebugPipeline_PCS{
          .model      = mesh.model,
          .solidColor = solidColor,
          .renderMode = renderMode,
          .objIndex   = objIdx,
      };
      pipeline->pushConstant(&pushConstant1, frame.commandBuffer);
      mesh.geometry->draw(frame.commandBuffer);
    }
  }

  // Render terrain with debug mode pushed to its shader
  if(auto* terrain = mScene->getTerrain())
  {
    const auto cameraData = mScene->getCamera().getCameraData();
    terrain->updateTessellationData(cameraData, mTessellationFactor);
    terrain->onRender(frame, mConfig.mode);
  }

  // Hack to only render in wireframe mode
  if(mConfig.mode == DebugRenderMode::eWireframe)
  {
    for(const auto& object : mScene->getGameObjects())
    {
      object->onRenderDebug(frame);
    }
  }

  frame.commandBuffer.endRendering();

  if(mEdgeDetect && (mConfig.mode == DebugRenderMode::eDepthEdges))
  {
    mEdgeDetect->setThreshold(mEdgeThreshold);
    const auto cameraData = mScene->getCamera().getCameraData();
    mEdgeDetect->setClipPlanes(cameraData.nearPlane, cameraData.farPlane);
    mEdgeDetect->render(frame);
  }

  if(mIsFirstRender)
  {
    mIsFirstRender = false;
  }
}

void DebugLayer::onDrawUI() noexcept
{
  if(mShowOptions)
  {
    ImGui::Begin("Options");

    ImGui::Text("Hint: Toggle with [O]");
    ImGui::SeparatorText("Terrain");

    ImGui::Text("Tessellation factor:");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##tessellationFactor", &mTessellationFactor, 0.01f, 2.0f, "%.2f");

    ImGui::SeparatorText("Camera");

    ImGui::Text("Orbit camera distance:");
    ImGui::SetNextItemWidth(-1.0f);
    if(auto* orbitCamera = dynamic_cast<OrbitCamera*>(&mScene->getCamera()))
    {
      mOrbitCameraDistance = orbitCamera->getDistance();
    }
    if(ImGui::SliderFloat("##orbitCameraDistance", &mOrbitCameraDistance, 3.0f, 100.0f, "%.1f"))
    {
      if(auto* orbitCamera = dynamic_cast<OrbitCamera*>(&mScene->getCamera()))
      {
        orbitCamera->setDistance(mOrbitCameraDistance);
      }
    }

    ImGui::Text("Current camera: %s", mIsFreeCamera ? "Free" : "Orbit");
    if(ImGui::Button("Toggle camera"))
      toggleCamera();

    ImGui::SeparatorText("Sun light");

    static float sunAzimuth   = mScene->getSunAzimuth();
    static float sunElevation = mScene->getSunElevation();

    ImGui::Text("Azimuth:");
    ImGui::SetNextItemWidth(-1.0f);
    if(ImGui::SliderFloat("##sunAzimuth", &sunAzimuth, 0.0f, 360.0f, "%.1f°"))
      mScene->setSunDirection(sunAzimuth, sunElevation);

    ImGui::Text("Elevation:");
    ImGui::SetNextItemWidth(-1.0f);
    if(ImGui::SliderFloat("##sunElevation", &sunElevation, 0.0f, 90.0f, "%.1f°"))
      mScene->setSunDirection(sunAzimuth, sunElevation);

    ImGui::SeparatorText("Edge Detection");

    ImGui::Text("Threshold:");
    ImGui::SetNextItemWidth(-1.0f);
    if(ImGui::SliderFloat("##edgeThreshold", &mEdgeThreshold, 0.000001f, 0.0005f, "%.6f"))
    {
      if(mEdgeDetect)
        mEdgeDetect->setThreshold(mEdgeThreshold);
    }

    ImGui::End();
  }
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

  if(ImGui::SmallButton("Wireframe"))
  {
    mConfig.mode = DebugRenderMode::eWireframe;
  }
  ImGui::SameLine();
  ImGui::Text("Show wireframe");

  if(ImGui::SmallButton("Depth"))
  {
    mConfig.mode = DebugRenderMode::eDepthEdges;
  }
  ImGui::SameLine();
  ImGui::Text("Show depth buffer");

  ImGui::End();
}

void DebugLayer::toggleCamera() noexcept
{
  auto& currentCamera = mScene->getCamera();

  if(!mIsFreeCamera)
  {
    const CameraData data = currentCamera.getCameraData();

    const glm::vec3 eyePos  = glm::vec3(data.eye);
    const glm::vec3 forward = glm::normalize(-glm::vec3(data.view[0][2], data.view[1][2], data.view[2][2]));
    const float     pitch   = std::asin(forward.y);
    const float     yaw     = std::atan2(forward.x, -forward.z);
    const float     fov     = 2.0f * std::atan(1.0f / std::abs(data.proj[1][1]));
    const auto [w, h]       = mVulkanContext->getSwapchain()->getExtent();
    const float aspect      = static_cast<float>(w) / static_cast<float>(h);
    auto        freeCam     = makeUnique<FreeCamera>(aspect, glm::degrees(fov), data.nearPlane, data.farPlane);
    freeCam->setYaw(yaw);
    freeCam->setPitch(pitch);
    freeCam->setPosition(eyePos);
    mOrbitCamera  = mScene->replaceCamera(std::move(freeCam));
    mIsFreeCamera = true;
  }
  else
  {
    mFreeCamera   = mScene->replaceCamera(std::move(mOrbitCamera));
    mIsFreeCamera = false;
  }
}

bool DebugLayer::isEnabled() const noexcept
{
  return mEnabled;
}

void DebugLayer::createDebugPipeline() noexcept
{
  using enum vk::ShaderStageFlagBits;
  mDepthBuffer = rhi::Image::create({
                                        .extent = mVulkanContext->getSwapchain()->getExtent(),
                                        .usageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                                        .format     = vk::Format::eD32Sfloat,
                                        .mipmapping = false,
                                        .samples    = vk::SampleCountFlagBits::e1,
                                        .label      = "BasicPipeline_DepthBuffer",
                                        .device     = mVulkanContext->getDevice(),
                                    })
                     .value();

  // Red contour edges in debug view
  mEdgeDetect = makeUnique<EdgeDetect>(mVulkanContext, mDepthBuffer, glm::vec3(1.0f, 0.0f, 0.0f), true);
  mEdgeDetect->setThreshold(mEdgeThreshold);

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

void DebugLayer::createWireframePipeline() noexcept
{
  using enum vk::ShaderStageFlagBits;
  mWireframePipeline = rhi::GraphicsPipelineBuilder()
                           .addDescriptor(0, mScene->getDescriptor())
                           .addPushConstantRange({eVertex | eFragment, 0, sizeof(DebugPipeline_PCS)})
                           .addVertexType<Vertex>()
                           .addShader({"assets/shaders/debug.vert.glsl", eVertex})
                           .addShader({"assets/shaders/debug.frag.glsl", eFragment})
                           .addAttachment(vk::Format::eB8G8R8A8Unorm)
                           .setDepthFormat(vk::Format::eD32Sfloat)
                           .configure([](rhi::GraphicsPipelineState& state) {
                             state.rasterizationState.setPolygonMode(vk::PolygonMode::eLine);
                           })
                           .setName("DebugWireframePipeline")
                           .create(mVulkanContext->getDevice());
}

}  // namespace ptvc
