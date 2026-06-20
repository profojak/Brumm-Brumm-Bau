#pragma once

#include <string>
#include <vector>
#include <SDL3/SDL_events.h>

#include "Geometry.hpp"
#include "Transform.hpp"
#include "VulkanCore.hpp"
#include "render/Pipeline.hpp"

namespace ptvc {
struct GameObjectParams
{
  SPtr<rhi::Pipeline> pipeline          = nullptr;
  SPtr<Geometry>      geometry          = nullptr;
  std::string         name              = "Unknown Object";
  Transform           initialTransform  = {};
  bool                useExampleTexture = false;
};

struct GPUGameObjectData
{
  glm::mat4 model;
  glm::vec4 solidColor;
  glm::vec4 materialProperties;  // ka, kd, ks, alpha
  int32_t   showFresnel;
  int32_t   useExampleTexture;
};

/**
     * GameObject base class
     */
class GameObject
{
public:
  explicit GameObject(const GameObjectParams& params)
      : mUseExampleTexture(params.useExampleTexture)
      , mTransform(params.initialTransform)
      , mGeometry(params.geometry)
      , mPipeline(params.pipeline)
      , mName(params.name)
  {
  }

  virtual ~GameObject() = default;

  // Handle events (e.g. mouse, key)
  virtual void onEvent(const SDL_Event& event) noexcept {}

  // Handle updates
  virtual void onUpdate(float dt, const rhi::Frame& frame) noexcept {}

  // Render the object
  virtual void onRender(const rhi::Frame& frame) noexcept {}

  // Render the object into the shadow map from the light's perspective
  virtual void onRenderShadow(const rhi::Frame& frame, rhi::Pipeline& shadowPipeline, const glm::mat4& lightVP) noexcept
  {
    if(!mGeometry)
      return;

    struct alignas(16) ShadowPushConstants
    {
      glm::mat4 model;
      glm::mat4 lightVP;
    };
    const ShadowPushConstants pc{mTransform.getModel(), lightVP};

    shadowPipeline.bind(frame, frame.commandBuffer);
    shadowPipeline.pushConstant(&pc, frame.commandBuffer);
    mGeometry->draw(frame.commandBuffer);
  }

  virtual void onRenderDebug(const rhi::Frame& frame) noexcept {}

  [[nodiscard]] const std::string& getName() const noexcept { return mName; }

  [[nodiscard]] GPUGameObjectData getGPUData() const noexcept
  {
    return {
        .model             = mTransform.getModel(),
        .solidColor        = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f),
        .useExampleTexture = mUseExampleTexture ? 1 : 0,
    };
  }

  [[nodiscard]] Geometry* getGeometry() const noexcept { return mGeometry.get(); }

  /// A single mesh to draw in the debug renderer, paired with the model
  /// matrix it should be drawn with.
  struct DebugMesh
  {
    glm::mat4 model;
    Geometry* geometry = nullptr;
  };

  /// Collect every mesh this object contributes to the debug view together
  /// with the world-space model matrix it should be rendered with. The base
  /// implementation emits the single base-class geometry (if any). Subclasses
  /// that own multiple meshes (e.g. vehicle body + wheels, checkpoint cylinder
  /// + star) override this so the debug renderer draws all of them at the
  /// correct size and transform.
  virtual void collectDebugMeshes(std::vector<DebugMesh>& out) const noexcept
  {
    if(mGeometry)
      out.push_back({mTransform.getModel(), mGeometry.get()});
  }

protected:
  bool                mUseExampleTexture;
  Transform           mTransform;
  SPtr<Geometry>      mGeometry;
  SPtr<rhi::Pipeline> mPipeline;

private:
  std::string mName;
};
}  // namespace ptvc
