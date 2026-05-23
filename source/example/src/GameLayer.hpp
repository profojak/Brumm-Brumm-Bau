#pragma once

#include <random>
#include <core/Layer.hpp>
#include <spdlog/spdlog.h>
#include <vulkan/VulkanContext.hpp>
#include <vulkan/render/Pipeline.hpp>

#include "Image.hpp"
#include "core/Application.hpp"
#include "scene/Scene.hpp"
#include "scene/primitives/Cube.hpp"

class GameLayer : public ptvc::ILayer
{
public:
  GameLayer();

  ~GameLayer() override = default;

  void onEvent(const SDL_Event& event) noexcept override;

  void onUpdate(float deltaTime) noexcept override {}

  void onRender(const ptvc::rhi::Frame& frame) noexcept override;

private:
  void createBasicPipelines() noexcept;

  /**
     * Example: Load an image from disk, upload it to the GPU
     * as an Image for use as a Texture and create a Sampler.
     * Creates a new descriptor containing the Texture in binding one
     * as a "CombinedImageSampler".
     */
  void loadTestTexture() noexcept;

  std::mt19937 mEngine;

  SPtr<ptvc::rhi::Image>      mTestTexture;
  vk::Sampler                 mSampler;
  SPtr<ptvc::rhi::Descriptor> mTextureDescriptor;

  bool                      mFirstRender = true;
  SPtr<ptvc::rhi::Pipeline> mPipeline;
  SPtr<ptvc::rhi::Pipeline> mPhongPipeline;
  SPtr<ptvc::rhi::Image>    mDepthBuffer;

  SPtr<ptvc::Geometry> mCubeGeometry;

  ptvc::Scene*                   mScene;
  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;
  SPtr<spdlog::logger>           mLogger;
};
