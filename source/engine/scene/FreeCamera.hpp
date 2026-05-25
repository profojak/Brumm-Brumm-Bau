#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ICamera.hpp"

namespace ptvc {
/**
* FPS Camera implementation.
* Controls:
* - Left MB: Look around
* - W/A/S/D: Move forward/left/back/right
* - Q/E: Move down/up
*/
class FreeCamera : public ICamera
{
public:
  /*!
   * Camera constructor
   * @param aspect: Aspect ratio
   * @param fov: Field of view, in degrees
   * @param near: Near plane distance
   * @param far: Far plane distance
   */
  explicit FreeCamera(float aspect, float fov = 45.0f, float near = 0.01f, float far = 256.0f);

  ~FreeCamera() override = default;

  /**
   * @param yaw New "yaw" value
   */
  void setYaw(float yaw) noexcept;

  /**
   * @param pitch New "pitch" value
   */
  void setPitch(float pitch) noexcept;

  /**
   * @param position New camera position
   */
  void setPosition(const glm::vec3& position) noexcept;

  /**
   * Get camera uniform data. (ICamera interface)
   */
  [[nodiscard]] CameraData getCameraData(float aspect) noexcept override;

  /**
   * Get camera uniform data.
   */
  [[nodiscard]] CameraData getCameraData() noexcept override;

  /**
   * Handle mouse (button and scroll) and keyboard events
   * @param event
   */
  void onEvent(const SDL_Event& event) noexcept override;

  /**
   * Update camera movement based on key states
   * @param deltaTime Time since last frame in seconds
   */
  void onUpdate(float deltaTime) noexcept override;

private:
  /**
   * Updates the camera's position and view matrix according to the input
   * @param x: current mouse x position
   * @param y: current mouse y position
   * @param isDragging: is the camera being dragged (left mouse button held)
   */
  void update(float x, float y, bool isDragging) noexcept;

  /**
   * Recompute the view matrix from current yaw, pitch, and position.
   */
  void recomputeViewMatrix() noexcept;

  float mFOV  = 65.0f;
  float mNear = 0.01f;
  float mFar  = 256.0f;
  float aspect;

  float mMouseX, mMouseY;
  float mYaw, mPitch;

  glm::vec3 mPosition;

  // Keyboard state
  bool mKeyW = false;
  bool mKeyA = false;
  bool mKeyS = false;
  bool mKeyD = false;
  bool mKeyQ = false;
  bool mKeyE = false;

  glm::mat4 mViewMatrix;
  glm::mat4 mProjMatrix = {};
};

}  // namespace ptvc
