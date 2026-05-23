#pragma once

#include <spdlog/spdlog.h>
#include <vulkan/VulkanContext.hpp>
#include <core/Layer.hpp>
#include <scene/Scene.hpp>

class GameLayer : public ptvc::ILayer
{
public:
  GameLayer();

  ~GameLayer() override = default;

  void onEvent(const SDL_Event& event) noexcept override;

  void onUpdate(float deltaTime) noexcept override;

  void onRender(const ptvc::rhi::Frame& frame) noexcept override;

private:
  ptvc::Scene*                   mScene;
  SPtr<ptvc::rhi::VulkanContext> mVulkanContext;
  SPtr<spdlog::logger>           mLogger;
};
