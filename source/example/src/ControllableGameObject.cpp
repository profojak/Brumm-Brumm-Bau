#include "ControllableGameObject.hpp"

ControllableGameObject::ControllableGameObject(const ptvc::GameObjectParams& params)
    : GameObject(params)
{
}

void ControllableGameObject::onEvent(const SDL_Event& event) noexcept
{
  switch(event.type)
  {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      const auto& keyEvent = event.key;
      const bool  pressed  = (keyEvent.type == SDL_EVENT_KEY_DOWN);
      switch(keyEvent.scancode)
      {
        case SDL_SCANCODE_UP: {
          mKeyState.z_bwd = pressed;
          break;
        }
        case SDL_SCANCODE_DOWN: {
          mKeyState.z_fwd = pressed;
          break;
        }
        case SDL_SCANCODE_LEFT: {
          mKeyState.x_bwd = pressed;
          break;
        }
        case SDL_SCANCODE_RIGHT: {
          mKeyState.x_fwd = pressed;
          break;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
}

void ControllableGameObject::onUpdate(const float dt, const ptvc::rhi::Frame& frame) noexcept
{
  if(mKeyState.x_fwd)
  {
    mTransform.translate += glm::vec3(mSpeed, 0.0f, 0.0f) * dt;
  }
  if(mKeyState.x_bwd)
  {
    mTransform.translate -= glm::vec3(mSpeed, 0.0f, 0.0f) * dt;
  }
  if(mKeyState.z_fwd)
  {
    mTransform.translate += glm::vec3(0.0f, 0.0f, mSpeed) * dt;
  }
  if(mKeyState.z_bwd)
  {
    mTransform.translate -= glm::vec3(0.0f, 0.0f, mSpeed) * dt;
  }
}

void ControllableGameObject::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  const ptvc::GPUGameObjectData pushConstant = {
      .model              = mTransform.getModel(),
      .solidColor         = {0.7f, 0.2f, 0.8f, 1.0f},
      .materialProperties = {0.1f, 0.7f, 0.2f, 10.0f},
      .showFresnel        = 0,
      .useExampleTexture  = mUseExampleTexture,
  };

  mPipeline->bind(frame, frame.commandBuffer);
  mPipeline->pushConstant(&pushConstant, frame.commandBuffer);

  mGeometry->draw(frame.commandBuffer);
}
