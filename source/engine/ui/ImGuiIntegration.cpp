#include "ImGuiIntegration.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

namespace ptvc::ui {
Result<UPtr<ImGuiIntegration>> ImGuiIntegration::create(const ImGuiIntegrationParams& params) noexcept
{
  auto result = UPtr<ImGuiIntegration>(new ImGuiIntegration(params));

  auto initResult = result->init_VulkanResources();
  if(!initResult.has_value())
  {
    return std::unexpected(initResult.error());
  }

  initResult = result->init_ImGui();
  if(!initResult.has_value())
  {
    return std::unexpected(initResult.error());
  }

  return result;
}

ImGuiIntegration::~ImGuiIntegration()
{
  mDevice->waitIdle();

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  mDevice->getHandle().destroy(mDescriptorPool);
}

void ImGuiIntegration::render(const rhi::Frame& frame, const std::function<void()>& uiDraws) noexcept
{
  const auto attachment = vk::RenderingAttachmentInfo()
                              .setClearValue(vk::ClearValue().setColor({0.0f, 0.0f, 0.0f, 0.0f}))
                              .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                              .setImageView(mRHI->getSwapchain()->getImageView(frame.acquiredImageIndex))
                              .setLoadOp(vk::AttachmentLoadOp::eLoad)
                              .setStoreOp(vk::AttachmentStoreOp::eStore);
  mRenderingInfo.setRenderArea({{0, 0}, mRHI->getSwapchain()->getExtent()}).setColorAttachments(attachment);

  const auto barrier = mRHI->getSwapchain()->getBarrier(
      frame.acquiredImageIndex, {
                                    .layout = vk::ImageLayout::eColorAttachmentOptimal,
                                    .accessMask = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
                                    .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                });
  const auto dependencyInfo = vk::DependencyInfo().setImageMemoryBarrierCount(1).setPImageMemoryBarriers(&barrier);

  frame.commandBuffer.beginDebugUtilsLabelEXT(mDebugLabel);
  frame.commandBuffer.pipelineBarrier2(dependencyInfo);
  frame.commandBuffer.beginRendering(mRenderingInfo);
  {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
    uiDraws();
    ImGui::EndFrame();

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, frame.commandBuffer);
  }
  frame.commandBuffer.endRendering();
  frame.commandBuffer.endDebugUtilsLabelEXT();
}

void ImGuiIntegration::onEvent(const SDL_Event& event) noexcept
{
  ImGui_ImplSDL3_ProcessEvent(&event);
}

bool ImGuiIntegration::wantCaptureInput() noexcept
{
  return wantCaptureMouse() || wantCaptureKeyboard();
}

ImGuiIntegration::ImGuiIntegration(const ImGuiIntegrationParams& params)
    : mWindow(params.window)
    , mRHI(params.vulkanContext)
    , mDevice(params.vulkanContext->getDevice())
{
  mDebugLabel = vk::DebugUtilsLabelEXT().setColor(std::array{0.8235f, 0.0588f, 0.2235f, 1.0f}).setPLabelName("ImGuiPass");
}

Result<void> ImGuiIntegration::init_VulkanResources() noexcept
{
  constexpr auto poolSize =
      vk::DescriptorPoolSize().setDescriptorCount(IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE).setType(vk::DescriptorType::eCombinedImageSampler);

  const auto descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo()
                                            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                                            .setMaxSets(poolSize.descriptorCount)
                                            .setPPoolSizes(&poolSize)
                                            .setPoolSizeCount(1);

  VK_RESULT(mDevice->getHandle().createDescriptorPool(&descriptorPoolCreateInfo, nullptr, &mDescriptorPool));

  mRenderingInfo = vk::RenderingInfo().setColorAttachmentCount(1).setLayerCount(1).setRenderArea(
      {{0, 0}, mRHI->getSwapchain()->getExtent()});

  return {};
}

Result<void> ImGuiIntegration::init_ImGui() const noexcept
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  const ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF("assets/fonts/GeistMono-Regular.ttf", 16.0f);

  ImGui::StyleColorsDark();

  const auto  displayScale = mWindow->getDisplayScale();
  ImGuiStyle& style        = ImGui::GetStyle();
  style.ScaleAllSizes(displayScale);
  style.FontScaleDpi = displayScale;

  auto bResult = ImGui_ImplSDL3_InitForVulkan(mWindow->getHandle());
  if(!bResult)
  {
    return std::unexpected("Failed to initialize ImGui with SDL3 for Vulkan");
  }

  const auto format = static_cast<VkFormat>(mRHI->getSwapchain()->getFormat());

  ImGui_ImplVulkan_InitInfo initInfo                    = {};
  initInfo.ApiVersion                                   = VK_API_VERSION_1_4;
  initInfo.Instance                                     = mRHI->getInstance();
  initInfo.PhysicalDevice                               = mDevice->getPhysicalDevice();
  initInfo.Device                                       = mDevice->getHandle();
  initInfo.QueueFamily                                  = mDevice->getGraphicsQueue().familyIndex;
  initInfo.Queue                                        = mDevice->getGraphicsQueue().getHandle();
  initInfo.PipelineCache                                = mPipelineCache;
  initInfo.DescriptorPool                               = mDescriptorPool;
  initInfo.MinImageCount                                = mRHI->getSwapchain()->getImageCount();
  initInfo.ImageCount                                   = mRHI->getSwapchain()->getImageCount();
  initInfo.UseDynamicRendering                          = true;
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {
      .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext                   = nullptr,
      .viewMask                = {},
      .colorAttachmentCount    = 1,
      .pColorAttachmentFormats = &format,
      .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  bResult = ImGui_ImplVulkan_Init(&initInfo);
  if(!bResult)
  {
    return std::unexpected("Failed to initialize ImGui Vulkan backend");
  }

  return {};
}

bool ImGuiIntegration::wantCaptureMouse() noexcept
{
  const ImGuiIO& io = ImGui::GetIO();
  return io.WantCaptureMouse;
}

bool ImGuiIntegration::wantCaptureKeyboard() noexcept
{
  const ImGuiIO& io = ImGui::GetIO();
  return io.WantCaptureKeyboard;
}
}  // namespace ptvc::ui
