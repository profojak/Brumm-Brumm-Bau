#include "FreeCamera.hpp"
#include "scene/ICamera.hpp"

namespace ptvc {
FreeCamera::FreeCamera(const float aspect, const float fov, const float near, const float far)
    : aspect(aspect)
    , mMouseX(0.0f)
    , mMouseY(0.0f)
    , mYaw(0.0f)
    , mPitch(0.0f)
    , mPosition(glm::vec3(0.0f))
    , mViewMatrix(glm::mat4(1))
{
  mProjMatrix = glm::perspective(glm::radians(fov), aspect, near, far);
}

void FreeCamera::setYaw(const float yaw) noexcept
{
  mYaw = yaw;
}

void FreeCamera::setPitch(const float pitch) noexcept
{
  mPitch = pitch;
}

CameraData FreeCamera::getCameraData(const float aspect) noexcept
{
  mProjMatrix = glm::perspective(glm::radians(mFOV), aspect, mNear, mFar);
  mProjMatrix[1][1] *= -1.0f;

  return {
      .view        = mViewMatrix,
      .proj        = mProjMatrix,
      .viewInverse = glm::inverse(mViewMatrix),
      .projInverse = glm::inverse(mProjMatrix),
      .eye         = {mPosition.x, mPosition.y, mPosition.z, 1.0f},
      .nearPlane   = mNear,
      .farPlane    = mFar,
  };
}

CameraData FreeCamera::getCameraData() noexcept
{
  return getCameraData(aspect);
}

void FreeCamera::onEvent(const SDL_Event& event) noexcept
{
  static bool isDragging = false;

  switch(event.type)
  {
    case SDL_EVENT_KEY_DOWN: {
      if(event.key.repeat)
      {
        break;
      }
      switch(event.key.scancode)
      {
        case SDL_SCANCODE_W:
          mKeyW = true;
          break;
        case SDL_SCANCODE_A:
          mKeyA = true;
          break;
        case SDL_SCANCODE_S:
          mKeyS = true;
          break;
        case SDL_SCANCODE_D:
          mKeyD = true;
          break;
        case SDL_SCANCODE_Q:
          mKeyQ = true;
          break;
        case SDL_SCANCODE_E:
          mKeyE = true;
          break;
        default:
          break;
      }
      break;
    }
    case SDL_EVENT_KEY_UP: {
      switch(event.key.scancode)
      {
        case SDL_SCANCODE_W:
          mKeyW = false;
          break;
        case SDL_SCANCODE_A:
          mKeyA = false;
          break;
        case SDL_SCANCODE_S:
          mKeyS = false;
          break;
        case SDL_SCANCODE_D:
          mKeyD = false;
          break;
        case SDL_SCANCODE_Q:
          mKeyQ = false;
          break;
        case SDL_SCANCODE_E:
          mKeyE = false;
          break;
        default:
          break;
      }
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      if(event.button.button == SDL_BUTTON_LEFT)
      {
        isDragging = true;
      }
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      if(event.button.button == SDL_BUTTON_LEFT)
      {
        isDragging = false;
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      update(event.motion.x, event.motion.y, isDragging);
      break;
    }
    default: {
      break;
    }
  }
}

void FreeCamera::onUpdate(const float deltaTime) noexcept
{
  if(!mKeyW && !mKeyA && !mKeyS && !mKeyD && !mKeyQ && !mKeyE)
  {
    return;
  }

  const glm::vec3 forward = glm::vec3(glm::sin(mYaw), 0.0f, -glm::cos(mYaw));
  const glm::vec3 right   = glm::vec3(glm::cos(mYaw), 0.0f, glm::sin(mYaw));
  const glm::vec3 up      = glm::vec3(0.0f, 1.0f, 0.0f);

  const float speed = 15.0f * deltaTime;
  glm::vec3   movement(0.0f);

  if(mKeyW)
    movement += forward;
  if(mKeyS)
    movement -= forward;
  if(mKeyD)
    movement += right;
  if(mKeyA)
    movement -= right;
  if(mKeyE)
    movement += up;
  if(mKeyQ)
    movement -= up;

  mPosition += movement * speed;

  recomputeViewMatrix();
}

void FreeCamera::update(const float x, const float y, const bool isDragging) noexcept
{
  const float     dx    = x - mMouseX;
  const float     dy    = y - mMouseY;
  constexpr float speed = 0.005f;

  if(isDragging)
  {
    mYaw += dx * speed;
    mPitch -= dy * speed;

    mPitch = glm::clamp(mPitch, -glm::pi<float>() * 0.5f + 0.01f, glm::pi<float>() * 0.5f - 0.01f);
  }

  recomputeViewMatrix();

  mMouseX = x;
  mMouseY = y;
}

void FreeCamera::recomputeViewMatrix() noexcept
{
  const glm::vec3 forward(glm::cos(mPitch) * glm::sin(mYaw), glm::sin(mPitch), -glm::cos(mPitch) * glm::cos(mYaw));
  mViewMatrix = glm::lookAt(mPosition, mPosition + forward, glm::vec3(0.0f, 1.0f, 0.0f));
}
}  // namespace ptvc
