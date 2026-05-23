#pragma once

#include <string>
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

protected:
  bool                mUseExampleTexture;
  Transform           mTransform;
  SPtr<Geometry>      mGeometry;
  SPtr<rhi::Pipeline> mPipeline;

private:
  std::string mName;
};
}  // namespace ptvc
