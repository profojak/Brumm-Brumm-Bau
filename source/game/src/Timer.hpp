#pragma once

#include <chrono>

#include <scene/GameObject.hpp>
#include <scene/Geometry.hpp>
#include <vulkan/VulkanContext.hpp>
#include <Descriptor.hpp>
#include <Image.hpp>

class Checkpoints;

// HUD
class Timer : public ptvc::GameObject
{
public:
  Timer(const ptvc::GameObjectParams& params, SPtr<ptvc::rhi::VulkanContext> vulkanContext, Checkpoints* checkpoints);
  ~Timer() override;

  void onEvent(const SDL_Event& event) noexcept override;
  void onUpdate(float dt, const ptvc::rhi::Frame& frame) noexcept override;
  void onRender(const ptvc::rhi::Frame& frame) noexcept override;
  void onRenderDebug(const ptvc::rhi::Frame& frame) noexcept override;

private:
  void loadTextTexture();
  void createTextGeometry();
  void createTextPipeline();
  void createWireframePipeline();
  void createTextDescriptor();

  void drawText(const ptvc::rhi::Frame&          frame,
                const char*                      str,
                float                            x,
                float                            y,
                float                            screenW,
                float                            screenH,
                const SPtr<ptvc::rhi::Pipeline>& pipeline) noexcept;

  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;
  Checkpoints*                   mCheckpoints;

  SPtr<ptvc::rhi::Image>      mTextTexture;
  vk::Sampler                 mSampler{};
  SPtr<ptvc::rhi::Descriptor> mDescriptor;
  SPtr<ptvc::rhi::Pipeline>   mPipeline;
  SPtr<ptvc::rhi::Pipeline>   mWireframePipeline;
  UPtr<ptvc::Geometry>        mGeometry;

  std::chrono::steady_clock::time_point mStartTime{};
  bool                                  mStarted = false;
  bool                                  mStopped = false;
  double                                mElapsed = 0.0;  // seconds

  // Glyph atlas description.
  static constexpr uint32_t kAtlasGlyphCount = 12;
  static constexpr uint32_t kGlyphWidth      = 9;
  static constexpr uint32_t kGlyphHeight     = 12;

  // On-screen scaling + layout.
  static constexpr float kGlyphScale = 4.0f;
  static constexpr float kMargin     = 12.0f;
  static constexpr float kGlyphGap   = 4.0f;

  // Maximum number of character slots
  static constexpr uint32_t kMaxSlots = 16;
};
