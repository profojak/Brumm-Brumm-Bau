#pragma once

#include <array>

#include <scene/GameObject.hpp>
#include <scene/glTF.hpp>
#include <vulkan/VulkanContext.hpp>
#include <Descriptor.hpp>
#include <Image.hpp>

#include <physics/VehiclePhysics.hpp>

namespace ptvc {
class Physics;
}

class Vehicle : public ptvc::GameObject
{
public:
  Vehicle(const ptvc::GameObjectParams&      params,
          SPtr<ptvc::Physics>                physics,
          SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
          const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  ~Vehicle() override;

  void onEvent(const SDL_Event& event) noexcept override;
  void onUpdate(float dt, const ptvc::rhi::Frame& frame) noexcept override;
  void onRender(const ptvc::rhi::Frame& frame) noexcept override;
  void onRenderShadow(const ptvc::rhi::Frame& frame, ptvc::rhi::Pipeline& shadowPipeline, const glm::mat4& lightVP) noexcept override;

  void collectDebugMeshes(std::vector<ptvc::GameObject::DebugMesh>& out) const noexcept override;

  [[nodiscard]] glm::vec3 getPosition() const;
  [[nodiscard]] glm::quat getRotation() const noexcept { return mTransform.rotation; }
  [[nodiscard]] bool      isAccelerating() const noexcept { return mForward; }

private:
  void                    createTextureDescriptor();
  void                    createRenderResources(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  [[nodiscard]] glm::mat4 wheelModelMatrix(int wheelIndex) const noexcept;

  static constexpr int       kWheelCount         = ptvc::VehiclePhysics::kWheelCount;
  static constexpr float     kMaxSteeringAngle   = ptvc::VehiclePhysics::MAX_STEERING_ANGLE;
  static constexpr float     kMaxRotationSpeed   = 31.415927f;
  static constexpr float     kModelScale         = 1.5f;
  static constexpr glm::vec3 kModelChassisCenter = {0.0f, 0.55f, -0.014f};

  SPtr<ptvc::Physics>            mPhysics;
  UPtr<ptvc::VehiclePhysics>     mVehiclePhysics;
  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;

  // Rendering resources
  SPtr<ptvc::rhi::Descriptor> mTextureDescriptor;
  SPtr<ptvc::rhi::Image>      mTexture;
  vk::Sampler                 mTextureSampler = nullptr;

  std::array<UPtr<ptvc::glTF>, kWheelCount> mWheelGeometry{};
  std::array<glm::mat4, kWheelCount>        mWheelBake{};

  bool mForward  = false;
  bool mBackward = false;
  bool mLeft     = false;
  bool mRight    = false;
};
