#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ptvc {
struct Transform
{
  glm::vec3 translate = glm::vec3(0.0f);
  glm::vec3 scale     = glm::vec3(1.0f);
  glm::quat rotation  = glm::quat(glm::vec3(0.0f));

  [[nodiscard]] glm::mat4 getModel() const noexcept
  {
    const glm::mat4 T = glm::translate(glm::mat4(1.0f), translate);
    const glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    const glm::mat4 R = glm::toMat4(rotation);
    return T * R * S;
  }
};
}  // namespace ptvc
