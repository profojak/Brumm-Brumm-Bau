#pragma once

#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ICamera.hpp"

namespace ptvc {
/**
 * Orbit Camera implementation — follows a target (e.g. the vehicle)
 * with spherical coordinates.
 *
 * Controls:
 * - Left MB drag: Rotate around target (azimuth / elevation)
 * - Mouse wheel:  Zoom in / out (change distance)
 */
class OrbitCamera : public ICamera
{
public:
  /**
   * @param aspect  Initial aspect ratio (updated each frame via getCameraData)
   * @param fov     Vertical field of view in degrees
   * @param near    Near plane distance
   * @param far     Far plane distance
   * @param distance Initial distance from target
   * @param azimuth  Initial horizontal angle in radians
   * @param elevation Initial vertical angle in radians
   */
  explicit OrbitCamera(float aspect,
                       float fov       = 65.0f,
                       float near      = 0.01f,
                       float far       = 512.0f,
                       float distance  = 15.0f,
                       float azimuth   = 0.0f,
                       float elevation = 0.5f);

  ~OrbitCamera() override = default;

  // Set a callback that returns the target position each frame (e.g. vehicle position).
  void setTargetCallback(std::function<glm::vec3()> callback) noexcept { mTargetCallback = std::move(callback); }

  // Directly set the target position (useful for a static target).
  void setTarget(const glm::vec3& target) noexcept { mManualTarget = target; }

  [[nodiscard]] CameraData getCameraData(float aspect) noexcept override;
  [[nodiscard]] CameraData getCameraData() noexcept override;

  void onEvent(const SDL_Event& event) noexcept override;
  void onUpdate(float deltaTime) noexcept override;

  void                setDistance(float distance) noexcept;
  [[nodiscard]] float getDistance() const noexcept;

private:
  void recomputeViewMatrix() noexcept;

  float mFOV    = 65.0f;
  float mNear   = 0.01f;
  float mFar    = 512.0f;
  float mAspect = 1.0f;

  // Spherical coordinates around the target
  float mDistance  = 15.0f;
  float mAzimuth   = 0.0f;
  float mElevation = 0.5f;

  // Target position
  std::function<glm::vec3()> mTargetCallback;
  glm::vec3                  mManualTarget = glm::vec3(0.0f);

  float mMouseX     = 0.0f;
  float mMouseY     = 0.0f;
  bool  mIsDragging = false;

  glm::mat4 mViewMatrix  = glm::mat4(1.0f);
  glm::mat4 mProjMatrix  = glm::mat4(1.0f);
  glm::vec3 mEyePosition = glm::vec3(0.0f);
};

}  // namespace ptvc
