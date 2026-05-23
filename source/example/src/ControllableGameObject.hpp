#pragma once

#include <scene/GameObject.hpp>

/**
 * Example GameObject that can be controlled by the "Player"
 * with movement restricted to the XZ plane.
 * Keybinds: Arrow Keys
 */
class ControllableGameObject : public ptvc::GameObject
{
public:
  explicit ControllableGameObject(const ptvc::GameObjectParams& params);

  ~ControllableGameObject() override = default;

  void onEvent(const SDL_Event& event) noexcept override;

  void onUpdate(float dt, const ptvc::rhi::Frame& frame) noexcept override;

  void onRender(const ptvc::rhi::Frame& frame) noexcept override;

private:
  struct
  {
    bool x_fwd = false;
    bool x_bwd = false;
    bool z_fwd = false;
    bool z_bwd = false;
  } mKeyState;

  float mSpeed = 0.5f;
};
