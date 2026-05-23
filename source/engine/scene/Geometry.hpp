#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Vertex.hpp"
#include "vulkan/Buffer.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ptvc {
class Geometry
{
public:
  explicit Geometry(std::string_view name = "Unknown");

  void init(const rhi::VulkanContext* pRHI);

  void draw(const vk::CommandBuffer& commandBuffer) const noexcept
  {
    constexpr vk::DeviceSize offsets[1] = {0};
    commandBuffer.bindVertexBuffers(0, 1, &mVertexBuffer->getHandle(), offsets);
    commandBuffer.bindIndexBuffer(mIndexBuffer->getHandle(), 0, vk::IndexType::eUint32);
    commandBuffer.drawIndexed(mIndices.size(), 1, 0, 0, 0);
  }

  [[nodiscard]] const std::vector<Vertex>& getVertices() const noexcept;

  [[nodiscard]] const SPtr<rhi::Buffer>& getVertexBuffer() const noexcept;

  [[nodiscard]] const std::vector<std::uint32_t>& getIndices() const noexcept;

  [[nodiscard]] const SPtr<rhi::Buffer>& getIndexBuffer() const noexcept;

  [[nodiscard]] std::string_view getName() const noexcept;

protected:
  std::vector<Vertex>        mVertices;
  std::vector<std::uint32_t> mIndices;

private:
  SPtr<rhi::Buffer> mVertexBuffer;
  SPtr<rhi::Buffer> mIndexBuffer;
  std::string       mName;
};
}  // namespace ptvc
