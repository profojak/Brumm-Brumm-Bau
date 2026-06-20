#pragma once

#include <glm/glm.hpp>

#include <vulkan/VulkanContext.hpp>
#include <Image.hpp>
#include <Descriptor.hpp>
#include <render/GraphicsPipeline.hpp>

/**
 * Post-processing pass that detects contour edges on the scene depth buffer
 * with a 3x3 Sobel operator and draws them over the color attachment.
 * The pass is executed once the whole scene has been rendered.
 */
class EdgeDetect
{
public:
  EdgeDetect(SPtr<ptvc::rhi::VulkanContext> vulkanContext, SPtr<ptvc::rhi::Image> depthBuffer, glm::vec3 edgeColor, bool visualizeDepth);
  ~EdgeDetect();

  EdgeDetect(const EdgeDetect&)            = delete;
  EdgeDetect& operator=(const EdgeDetect&) = delete;

  void render(const ptvc::rhi::Frame& frame) noexcept;

  void                setThreshold(float t) noexcept { mThreshold = t; }
  [[nodiscard]] float getThreshold() const noexcept { return mThreshold; }

  void setClipPlanes(float nearPlane, float farPlane) noexcept
  {
    mNearPlane = nearPlane;
    mFarPlane  = farPlane;
  }

private:
  void createResources() noexcept;

  struct PushConstants
  {
    glm::vec4 edgeColor;
    float     threshold;
    int32_t   visualizeDepth = 0;
    float     nearPlane      = 0.0f;
    float     farPlane       = 1.0f;
  };

  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;
  SPtr<ptvc::rhi::Image>         mDepthBuffer;
  SPtr<ptvc::rhi::Descriptor>    mDescriptor;
  UPtr<ptvc::rhi::Pipeline>      mPipeline;
  vk::Sampler                    mSampler = nullptr;

  glm::vec4 mEdgeColor;
  int32_t   mVisualizeDepth = 0;

  float mThreshold = 0.0001f;
  float mNearPlane = 0.0f;
  float mFarPlane  = 1.0f;
};
