#pragma once

#include <concepts>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace ptvc::rhi {
using VertexAttributes = std::vector<vk::VertexInputAttributeDescription>;
using VertexBinding    = vk::VertexInputBindingDescription;

/**
     * The GraphicsPipelineBuilder uses a templated method to register vertex attributes and bindings.
     * This concept helps validate compile-time that the vertex type passed as template parameter
     * has the static methods required.
     * ! Note that for compatibility with the GraphicsPipelineBuilder a vertex type needs to satisfy this concept.
     */
template <typename T>
concept VertexType = requires(T t, uint32_t u) {
  { T::getAttributes(u, u) } -> std::same_as<VertexAttributes>;
  { T::getBinding(u) } -> std::same_as<VertexBinding>;
  { T::getAttributeCount() } -> std::same_as<uint32_t>;
};

/**
     * Concept to validate index types.
     */
template <typename T>
concept IndexType = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t>;
}  // namespace ptvc::rhi
