#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <scene/GameObject.hpp>
#include <scene/Geometry.hpp>
#include <vulkan/Buffer.hpp>
#include <vulkan/VulkanContext.hpp>
#include <render/Pipeline.hpp>

class Vehicle;

/**
 * CPU exhaust gas particle system.
 */
class Exhaust : public ptvc::GameObject
{
public:
  Exhaust(const ptvc::GameObjectParams&      params,
          Vehicle*                           vehicle,
          SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
          const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);
  ~Exhaust() override = default;

  void onEvent(const SDL_Event& event) noexcept override {}
  void onUpdate(float dt, const ptvc::rhi::Frame& frame) noexcept override;
  void onRender(const ptvc::rhi::Frame& frame) noexcept override;

  // Exhaust gas does not cast shadows.
  void onRenderShadow(const ptvc::rhi::Frame&, ptvc::rhi::Pipeline&, const glm::mat4&) noexcept override {}

private:
  void createGeometry();
  void createInstanceBuffer();
  void createPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor);

  struct Particle
  {
    glm::vec3 position   = glm::vec3(0.0f);
    float     scale      = 0.0f;
    float     life       = 0.0f;
    float     maxLife    = 0.0f;
    float     startScale = 0.0f;
    glm::vec3 drift      = glm::vec3(0.0f);
  };

  static constexpr size_t kMaxParticles  = 21;
  static constexpr float  kSpawnInterval = 0.05f;
  static constexpr float  kLifetime      = 0.5f;
  static constexpr float  kStartScale    = 0.4f;
  static constexpr float  kRiseSpeed     = 0.4f;

  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;
  Vehicle*                       mVehicle = nullptr;

  UPtr<ptvc::Geometry>      mCube;
  SPtr<ptvc::rhi::Pipeline> mPipeline;
  SPtr<ptvc::rhi::Buffer>   mInstanceBuffer;

  std::array<Particle, kMaxParticles> mParticles{};
  float                               mSpawnTimer = 0.0f;
};
