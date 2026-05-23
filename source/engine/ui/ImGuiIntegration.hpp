#pragma once

#include <functional>
#include <lib/lib.hpp>
#include "vulkan/VulkanContext.hpp"
#include "wsi/sdl/SDLWindow.hpp"

namespace ptvc::ui {
struct ImGuiIntegrationParams
{
  SPtr<wsi::SDLWindow> window;
  rhi::VulkanContext*  vulkanContext;
};

class ImGuiIntegration
{
public:
  DISABLE_COPY(ImGuiIntegration);

  [[nodiscard]] static Result<UPtr<ImGuiIntegration>> create(const ImGuiIntegrationParams& params) noexcept;

  ~ImGuiIntegration();

  void render(const rhi::Frame& frame, const std::function<void()>& uiDraws) noexcept;

  void onEvent(const SDL_Event& event) noexcept;

  bool wantCaptureInput() noexcept;

private:
  explicit ImGuiIntegration(const ImGuiIntegrationParams& params);

  [[nodiscard]] Result<void> init_VulkanResources() noexcept;
  [[nodiscard]] Result<void> init_ImGui() const noexcept;

  bool wantCaptureMouse() noexcept;

  bool wantCaptureKeyboard() noexcept;


  // Vulkan resources for ImGui
  vk::DescriptorPool     mDescriptorPool;
  vk::PipelineCache      mPipelineCache;
  vk::RenderingInfo      mRenderingInfo;
  vk::DebugUtilsLabelEXT mDebugLabel;

  SPtr<wsi::SDLWindow> mWindow;
  rhi::VulkanContext*  mRHI;
  SPtr<rhi::Device>    mDevice;
};
}  // namespace ptvc::ui
