#include "Geometry.hpp"

namespace ptvc
{
    Geometry::Geometry(const std::string_view name)
    : mName(name)
    {
    }

    void Geometry::init(const rhi::VulkanContext* pRHI)
    {
        const auto vertexSize = sizeof(Vertex) * mVertices.size();
        /* Vertex Buffer */ {
            auto result = rhi::Buffer::create({
                .size        = vertexSize,
                .usageFlags  = vk::BufferUsageFlagBits::eVertexBuffer,
                .hostVisible = false,
                .label       = std::format("{}-VertexBuffer", mName),
                .device      = pRHI->getDevice()
            });
            result_moveOrExit(result, mVertexBuffer);
        }

        const auto indexSize = sizeof(std::uint32_t) * mIndices.size();
        /* Index Buffer */ {
            auto result = rhi::Buffer::create({
                .size        = indexSize,
                .usageFlags  = vk::BufferUsageFlagBits::eIndexBuffer,
                .hostVisible = false,
                .label       = std::format("{}-IndexBuffer", mName),
                .device      = pRHI->getDevice()
            });
            result_moveOrExit(result, mIndexBuffer);
        }

        const auto staging = rhi::Buffer::create({
            .size        = indexSize + vertexSize,
            .hostVisible = true,
            .device      = pRHI->getDevice(),
        });
        staging.value()->setData(mVertices.data(), vertexSize, 0);
       staging.value()->setData(mIndices.data(), indexSize, vertexSize);

        pRHI->executeImmediateCommand([&](const vk::CommandBuffer& commandBuffer) -> void {
            const auto vertexRegion = vk::BufferCopy2().setSrcOffset(0).setDstOffset(0).setSize(vertexSize);
            const auto vertexCopy = vk::CopyBufferInfo2()
                .setSrcBuffer(staging.value()->getHandle())
                .setDstBuffer(mVertexBuffer->getHandle())
                .setRegions(vertexRegion);
            commandBuffer.copyBuffer2(vertexCopy);

            const auto indexRegion = vk::BufferCopy2().setSrcOffset(vertexSize).setDstOffset(0).setSize(indexSize);
            const auto indexCopy = vk::CopyBufferInfo2()
                .setSrcBuffer(staging.value()->getHandle())
                .setDstBuffer(mIndexBuffer->getHandle())
                .setRegions(indexRegion);
            commandBuffer.copyBuffer2(indexCopy);
        });
    }

    const std::vector<Vertex>& Geometry::getVertices() const noexcept
    {
        return mVertices;
    }

    const SPtr<rhi::Buffer>& Geometry::getVertexBuffer() const noexcept
    {
        return mVertexBuffer;
    }

    const std::vector<std::uint32_t>& Geometry::getIndices() const noexcept
    {
        return mIndices;
    }

    const SPtr<rhi::Buffer>& Geometry::getIndexBuffer() const noexcept
    {
        return mIndexBuffer;
    }

    std::string_view Geometry::getName() const noexcept
    {
        return mName;
    }
}
