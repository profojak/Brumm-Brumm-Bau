#pragma once

#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include <scene/GameObject.hpp>
#include <scene/Geometry.hpp>
#include <scene/glTF.hpp>
#include <vulkan/VulkanContext.hpp>
#include <Descriptor.hpp>

class Checkpoints : public ptvc::GameObject
{
public:
  struct Checkpoint
  {
    uint32_t  index;
    glm::vec3 position;
  };

  Checkpoints(const ptvc::GameObjectParams&      params,
              SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
              const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  ~Checkpoints() override;

  void onEvent(const SDL_Event& event) noexcept override {}
  void onUpdate(float dt, const ptvc::rhi::Frame& frame) noexcept override;
  void onRender(const ptvc::rhi::Frame& frame) noexcept override;
  void onRenderShadow(const ptvc::rhi::Frame& frame, ptvc::rhi::Pipeline& shadowPipeline, const glm::mat4& lightVP) noexcept override
  {
  }

  void collectDebugMeshes(std::vector<ptvc::GameObject::DebugMesh>& out) const noexcept override;

  [[nodiscard]] const std::vector<Checkpoint>& getCheckpoints() const noexcept { return mCheckpoints; }

  /// Index of the currently active checkpoint (one shown at a time).
  /// Equals mCheckpoints.size() once all checkpoints have been reached.
  [[nodiscard]] size_t getCurrentCheckpointIndex() const noexcept { return mCurrentCheckpoint; }

  /// How many checkpoints have been collected so far.
  [[nodiscard]] size_t getCollectedCount() const noexcept { return std::min(mCurrentCheckpoint, mCheckpoints.size()); }

  [[nodiscard]] bool isFinished() const noexcept { return mCurrentCheckpoint >= mCheckpoints.size(); }

  /// Provide a way to query the vehicle's world position for star spin.
  void setVehiclePositionProvider(std::function<glm::vec3()> provider) noexcept
  {
    mVehiclePositionProvider = std::move(provider);
  }

private:
  void loadFromProps();
  void loadHeightmapAndPlaceOnTerrain();
  void createCylinderGeometry();
  void createCylinderPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  void loadStar();
  void createStarPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);

  static constexpr float kTerrainWorldSize = 310.0f;
  static constexpr float kTerrainHalfSize  = kTerrainWorldSize * 0.5f;
  static constexpr float kHeightScale      = 60.0f;

  static constexpr float     kCylinderRadius = 4.0f;
  static constexpr float     kCylinderHeight = 400.0f;
  static constexpr glm::vec4 kCylinderColor  = {1.0f, 0.85f, 0.2f, 0.25f};

  static constexpr float     kStarHoverHeight = 4.0f;
  static constexpr float     kStarScale       = 3.0f;
  static constexpr glm::vec3 kStarColor       = {1.0f, 0.93f, 0.25f};
  static constexpr float     kSpinBaseSpeed   = 1.0f;
  static constexpr float     kSpinBoostSpeed  = 9.0f;
  static constexpr float     kApproachRange   = 80.0f;
  static constexpr float     kReachDistance   = 8.0f;

  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;

  UPtr<ptvc::Geometry>      mCylinder;
  SPtr<ptvc::rhi::Pipeline> mCylinderPipeline;

  UPtr<ptvc::glTF>          mStar;
  SPtr<ptvc::rhi::Pipeline> mStarPipeline;

  std::vector<Checkpoint> mCheckpoints;
  std::vector<float>      mSpinAngles;
  size_t                  mCurrentCheckpoint = 0;

  std::function<glm::vec3()> mVehiclePositionProvider;
};
