#pragma once

#include <glm/glm.hpp>
#include "vulkan/render/VertexType.hpp"

namespace ptvc
{
    // Default Position, Normal and UV vertex that satisfies the "VertexType" concept.
    // Vertex types that satisfy "VertexType" can be easily registered for use in Graphics Pipelines,
    // see "vulkan/render/VertexType.hpp" for more info.
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        // Implement the "VertexType" required static functions.
        #pragma region

        [[nodiscard]] static rhi::VertexAttributes getAttributes(const uint32_t firstLoc = 0, const uint32_t binding = 0) noexcept
        {
            return {
                { firstLoc + 0, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position) },
                { firstLoc + 1, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal  ) },
                { firstLoc + 2, binding, vk::Format::eR32G32Sfloat,    offsetof(Vertex, uv      ) },
            };
        }

        [[nodiscard]] static rhi::VertexBinding getBinding(const uint32_t binding) noexcept
        {
            return { binding, sizeof(Vertex), vk::VertexInputRate::eVertex };
        }

        [[nodiscard]] static constexpr uint32_t getAttributeCount() noexcept
        {
            return 3;
        }

        #pragma endregion
    };
}
