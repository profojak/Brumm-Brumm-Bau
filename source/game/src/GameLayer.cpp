#include "GameLayer.hpp"

#include <core/Application.hpp>

GameLayer::GameLayer()
{
  const auto* app = ptvc::Application::getApplication();
  mVulkanContext  = app->getVulkanContext();
  mScene          = app->getScene();
}

void GameLayer::onEvent(const SDL_Event& event) noexcept {}

void GameLayer::onUpdate(const float deltaTime) noexcept {}

void GameLayer::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  // No rendering yet
}
