#pragma once

#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <glm/glm.hpp>

#include <scene/GameObject.hpp>
#include <scene/glTF.hpp>
#include <vulkan/VulkanContext.hpp>
#include <Descriptor.hpp>
#include <Image.hpp>
#include <Buffer.hpp>

namespace ptvc {
class Physics;
}

class Trees : public ptvc::GameObject
{
public:
  Trees(const ptvc::GameObjectParams&      params,
        SPtr<ptvc::Physics>                physics,
        SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
        const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  ~Trees() override;

  void onEvent(const SDL_Event& event) noexcept override {}
  void onUpdate(float dt, const ptvc::rhi::Frame& frame) noexcept override {}
  void onRender(const ptvc::rhi::Frame& frame) noexcept override;
  void onRenderShadow(const ptvc::rhi::Frame& frame, ptvc::rhi::Pipeline& shadowPipeline, const glm::mat4& lightVP) noexcept override;

  void collectDebugMeshes(std::vector<ptvc::GameObject::DebugMesh>& out) const noexcept override;

  [[nodiscard]] size_t getTreeCount() const noexcept { return mInstances.size(); }

private:
  struct TreeInstance
  {
    glm::mat4 model;
    glm::vec3 position;
    float     yaw   = 0.0f;
    float     scale = 1.0f;
  };

  void loadFromProps();
  void placeOnTerrain();
  void loadTreeModel();
  void createTreeTexture();
  void createInstanceBuffer();
  void createPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  void createShadowPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  void createPhysicsBodies();
  void destroyPhysicsBodies();

  static constexpr float kTerrainWorldSize = 310.0f;
  static constexpr float kTerrainHalfSize  = kTerrainWorldSize * 0.5f;
  static constexpr float kHeightScale      = 60.0f;

  static constexpr float kTreeMinScale = 1.5f;
  static constexpr float kTreeMaxScale = 3.0f;

  static constexpr float kTrunkHalfWidth  = 0.35f;
  static constexpr float kTrunkHalfHeight = 1.5f;
  static constexpr float kTrunkCenterY    = 1.5f;

  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;

  UPtr<ptvc::glTF>          mTree;
  SPtr<ptvc::rhi::Pipeline> mPipeline;
  SPtr<ptvc::rhi::Pipeline> mTreeShadowPipeline;

  SPtr<ptvc::rhi::Descriptor> mTextureDescriptor;
  SPtr<ptvc::rhi::Image>      mTexture;
  vk::Sampler                 mTextureSampler = nullptr;

  SPtr<ptvc::rhi::Buffer>   mInstanceBuffer;
  std::vector<TreeInstance> mInstances;

  SPtr<ptvc::Physics>      mPhysics;
  std::vector<JPH::BodyID> mTrunkBodyIds;
};
