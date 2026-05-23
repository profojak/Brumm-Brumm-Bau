#pragma once

#include <glm/glm.hpp>

namespace ptvc
{
    // Basic point light representation
    struct PointLight
    {
        glm::vec4 color;
        glm::vec4 position;
        glm::vec4 attenuation;
    };

    // Basic directional light representation
    struct DirectionalLight
    {
        glm::vec4 color;
        glm::vec4 direction;
    };
}
