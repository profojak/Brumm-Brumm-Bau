#pragma once

#include <glm/glm.hpp>
#include <SDL3/SDL_events.h>

namespace ptvc {
struct CameraData
{
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewInverse;
  glm::mat4 projInverse;
  glm::vec4 eye;
  float     nearPlane;
  float     farPlane;
};

class ICamera
{
public:
  virtual ~ICamera() = default;

  [[nodiscard]] virtual CameraData getCameraData(float aspect) noexcept = 0;

  virtual void onEvent(const SDL_Event& event) noexcept = 0;
  virtual void onUpdate(float deltaTime) noexcept       = 0;
};
}  // namespace ptvc
