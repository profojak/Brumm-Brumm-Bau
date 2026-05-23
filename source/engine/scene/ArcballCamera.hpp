#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "ICamera.hpp"

namespace ptvc {
/**
     * Arc ball Camera implementation.
     * Controls:
     * - Left MB: Look around
     * - Right MB: Strafe
     * - Scroll: Zoom in/out
     */
class ArcballCamera : public ICamera
{
public:
  /*!
         * Camera constructor
         * @param aspect: Aspect ratio
         * @param fov: Field of view, in degrees
         * @param near: Near plane distance
         * @param far: Far plane distance
         */
  explicit ArcballCamera(float aspect, float fov = 45.0f, float near = 0.01f, float far = 256.0f);

  ~ArcballCamera() override = default;

  /**
         * @param yaw New "yaw" value
         */
  void setYaw(float yaw) noexcept;

  /**
         * @param pitch New "pitch" value
         */
  void setPitch(float pitch) noexcept;

  /**
         * Get camera uniform data. (ICamera interface) 
         */
  [[nodiscard]] CameraData getCameraData(float aspect) noexcept override;

  /**
         * Handle mouse (button and scroll) events
         * @param event
         */
  void onEvent(const SDL_Event& event) noexcept override;

  void onUpdate(float deltaTime) noexcept override {}

private:
  /**
         * Updates the camera's position and view matrix according to the input
         * @param x: current mouse x position
         * @param y: current mouse y position
         * @param isDragging: is the camera isDragging
         * @param isStrafing: is the camera isStrafing
         */
  void update(float x, float y, bool isDragging, bool isStrafing) noexcept;

  float mFOV  = 75.0f;
  float mNear = 0.01f;
  float mFar  = 256.0f;

  float mMouseX, mMouseY;
  float mYaw, mPitch;

  float     mDistance = 5.0f;
  glm::vec3 mPosition;
  glm::vec3 mStrafe;

  glm::mat4 mViewMatrix;
  glm::mat4 mProjMatrix = {};
};

}  // namespace ptvc
