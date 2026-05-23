#pragma once

#include <glm/glm.hpp>
#include <scene/GameObject.hpp>

/**
 * Example GameObject that rotates around the given axis at some radians/s.
 */
class RotatingGameObject : public ptvc::GameObject
{
public:
  /**
     * Create a RotatingGameObject
     * @param rps Radians per Second
     * @param axis Rotation Axis
     * @param params GameObject base class params
     */
  RotatingGameObject(float rps, const glm::vec3& axis, const ptvc::GameObjectParams& params);

  ~RotatingGameObject() override = default;

  void onUpdate(float dt, const ptvc::rhi::Frame& frame) noexcept override;

  void onRender(const ptvc::rhi::Frame& frame) noexcept override;

private:
  glm::vec4       mColor;
  const glm::vec3 mRotationAxis;
  const float     mRadiansPerSec;
};
