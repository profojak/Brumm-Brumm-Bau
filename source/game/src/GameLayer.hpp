#pragma once

#include <spdlog/spdlog.h>
#include <vulkan/VulkanContext.hpp>
#include <core/Layer.hpp>
#include <scene/Scene.hpp>
#include <scene/Terrain.hpp>
#include <Image.hpp>
#include <render/EdgeDetect.hpp>

namespace ptvc {
class Physics;
}

class Vehicle;

class GameLayer : public ptvc::ILayer
{
public:
  GameLayer();

  ~GameLayer() override;

  void onEvent(const SDL_Event& event) noexcept override;

  void onFixedUpdate(float fixedDeltaTime) noexcept override;

  void onUpdate(float deltaTime) noexcept override;

  void onRender(const ptvc::rhi::Frame& frame) noexcept override;

private:
  void createDepthBuffer() noexcept;

  bool mFirstRender = true;

  ptvc::Scene*                   mScene;
  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;
  SPtr<spdlog::logger>           mLogger;

  SPtr<ptvc::rhi::Image> mDepthBuffer;

  UPtr<ptvc::Terrain> mTerrain;
  UPtr<EdgeDetect>    mEdgeDetect;

  // Physics
  SPtr<ptvc::Physics>        mPhysics;
  SPtr<ptvc::TerrainPhysics> mTerrainPhysics;
  Vehicle*                   mVehicle = nullptr;
};
