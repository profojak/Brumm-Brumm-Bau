#include "OrbitCamera.hpp"

namespace ptvc {

OrbitCamera::OrbitCamera(const float aspect, const float fov, const float near, const float far, const float distance, const float azimuth, const float elevation)
    : mFOV(fov)
    , mNear(near)
    , mFar(far)
    , mAspect(aspect)
    , mDistance(distance)
    , mAzimuth(azimuth)
    , mElevation(elevation)
{
  mProjMatrix = glm::perspective(glm::radians(mFOV), mAspect, mNear, mFar);
  recomputeViewMatrix();
}

CameraData OrbitCamera::getCameraData(const float aspect) noexcept
{
  mAspect     = aspect;
  mProjMatrix = glm::perspective(glm::radians(mFOV), mAspect, mNear, mFar);
  mProjMatrix[1][1] *= -1.0f;

  return {
      .view        = mViewMatrix,
      .proj        = mProjMatrix,
      .viewInverse = glm::inverse(mViewMatrix),
      .projInverse = glm::inverse(mProjMatrix),
      .eye         = {mEyePosition, 1.0f},
      .nearPlane   = mNear,
      .farPlane    = mFar,
  };
}

CameraData OrbitCamera::getCameraData() noexcept
{
  return getCameraData(mAspect);
}

void OrbitCamera::onEvent(const SDL_Event& event) noexcept
{
  switch(event.type)
  {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      if(event.button.button == SDL_BUTTON_LEFT)
      {
        mIsDragging = true;
        mMouseX     = static_cast<float>(event.button.x);
        mMouseY     = static_cast<float>(event.button.y);
      }
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      if(event.button.button == SDL_BUTTON_LEFT)
      {
        mIsDragging = false;
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      if(!mIsDragging)
        break;

      const float dx = static_cast<float>(event.motion.x) - mMouseX;
      const float dy = static_cast<float>(event.motion.y) - mMouseY;

      constexpr float sensitivity = 0.005f;
      mAzimuth += dx * sensitivity;
      mElevation -= dy * sensitivity;

      // Clamp elevation to avoid flipping at the poles
      mElevation = glm::clamp(mElevation, -glm::pi<float>() * 0.5f + 0.05f, glm::pi<float>() * 0.5f - 0.05f);

      mMouseX = static_cast<float>(event.motion.x);
      mMouseY = static_cast<float>(event.motion.y);

      recomputeViewMatrix();
      break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
      float scrollDelta = 0.0f;
      if(event.wheel.integer_y != 0)
      {
        scrollDelta = static_cast<float>(event.wheel.integer_y);
      }
      else if(event.wheel.y != 0.0f)
      {
        scrollDelta = event.wheel.y;
      }

      mDistance -= scrollDelta;
      mDistance = glm::clamp(mDistance, 3.0f, 100.0f);
      recomputeViewMatrix();
      break;
    }
    default:
      break;
  }
}

void OrbitCamera::onUpdate(const float /*deltaTime*/) noexcept
{
  recomputeViewMatrix();
}

void OrbitCamera::recomputeViewMatrix() noexcept
{
  const glm::vec3 target = mTargetCallback ? mTargetCallback() : mManualTarget;
  const glm::vec3 offset(glm::cos(mElevation) * glm::sin(mAzimuth) * mDistance, glm::sin(mElevation) * mDistance,
                         -glm::cos(mElevation) * glm::cos(mAzimuth) * mDistance);

  mEyePosition = target + offset;
  mViewMatrix  = glm::lookAt(mEyePosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void OrbitCamera::setDistance(const float distance) noexcept
{
  mDistance = glm::clamp(distance, 3.0f, 100.0f);
  recomputeViewMatrix();
}

float OrbitCamera::getDistance() const noexcept
{
  return mDistance;
}

}  // namespace ptvc
