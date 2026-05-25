#pragma once

#include <scene/GameObject.hpp>
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

  /** Returns the world-space position of the car body. */
  [[nodiscard]] glm::vec3 getPosition() const;

private:
  void createDummyTextureDescriptor();
  void createRenderResources(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);

  static constexpr float kMaxSteeringAngle = ptvc::VehiclePhysics::MAX_STEERING_ANGLE;
  static constexpr float kMaxRotationSpeed = 31.415927f;

  SPtr<ptvc::Physics>            mPhysics;
  UPtr<ptvc::VehiclePhysics>     mVehiclePhysics;
  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;

  // Rendering resources
  SPtr<ptvc::rhi::Descriptor> mDummyTextureDescriptor;
  SPtr<ptvc::rhi::Image>      mDummyTexture;
  vk::Sampler                 mDummySampler = nullptr;

  bool mForward  = false;
  bool mBackward = false;
  bool mLeft     = false;
  bool mRight    = false;
};
