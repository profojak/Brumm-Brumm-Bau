#include "Terrain.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#include <scene/Vertex.hpp>
#include <render/GraphicsPipeline.hpp>

namespace ptvc {

Terrain::Terrain(const SPtr<rhi::VulkanContext>& vulkanContext, const SPtr<rhi::Descriptor>& sceneDescriptor)
    : mVulkanContext(vulkanContext)
    , mSceneDescriptor(sceneDescriptor)
{
  generateBaseMesh();
  loadHeightmap();
  createTerrainDescriptor();
  createPipeline();
}

Terrain::~Terrain()
{
  if(mHeightmapSampler)
  {
    mVulkanContext->getDevice()->getHandle().destroySampler(mHeightmapSampler);
  }
}

void Terrain::updateTessellationData(const CameraData& cameraData, const float tessellationFactor, const float displacementFactor) noexcept
{
  mTessellationData.tessellationFactor = tessellationFactor;

  mTessellationUBO->setData(&mTessellationData, sizeof(TerrainTessellationData), 0);
}

void Terrain::onRender(const rhi::Frame& frame) const noexcept
{
  const vk::DeviceSize offsets[1] = {0};

  mPipeline->bind(frame, frame.commandBuffer);

  frame.commandBuffer.bindVertexBuffers(0, 1, &mVertexBuffer->getHandle(), offsets);
  frame.commandBuffer.bindIndexBuffer(mIndexBuffer->getHandle(), 0, vk::IndexType::eUint32);
  frame.commandBuffer.drawIndexed(mIndexCount, 1, 0, 0, 0);
}

void Terrain::generateBaseMesh() noexcept
{
  const uint32_t patchSize = 64;
  const float    wx        = 2.0f;
  const float    wy        = 2.0f;
  const float    uvScale   = 1.0f;

  const uint32_t      vertexCount = patchSize * patchSize;
  std::vector<Vertex> vertices(vertexCount);

  // Generate flat grid
  for(auto x = 0u; x < patchSize; x++)
  {
    for(auto y = 0u; y < patchSize; y++)
    {
      uint32_t index           = (x + y * patchSize);
      vertices[index].position = glm::vec3(x * wx + wx / 2.0f - (float)patchSize * wx / 2.0f, 0.0f,
                                           y * wy + wy / 2.0f - (float)patchSize * wy / 2.0f);
      vertices[index].normal   = glm::vec3(0.0f, 1.0f, 0.0f);
      vertices[index].uv       = glm::vec2((float)x / (patchSize - 1), (float)y / (patchSize - 1)) * uvScale;
    }
  }

  // Generate indices for quad patches
  const uint32_t w = (patchSize - 1);
  mIndexCount      = w * w * 4;
  auto indices     = std::vector<uint32_t>(mIndexCount);
  for(auto x = 0u; x < w; x++)
  {
    for(auto y = 0u; y < w; y++)
    {
      uint32_t index     = (x + y * w) * 4;
      indices[index]     = (x + y * patchSize);
      indices[index + 1] = indices[index] + patchSize;
      indices[index + 2] = indices[index + 1] + 1;
      indices[index + 3] = indices[index] + 1;
    }
  }

  // Create vertex buffer
  const auto vertexBufferSize = vertexCount * sizeof(Vertex);
  {
    auto result = rhi::Buffer::create({
        .size        = vertexBufferSize,
        .usageFlags  = vk::BufferUsageFlagBits::eVertexBuffer,
        .hostVisible = false,
        .label       = "Terrain-VertexBuffer",
        .device      = mVulkanContext->getDevice(),
    });
    exitOnError(result);
    mVertexBuffer = std::move(result.value());
  }

  // Create index buffer
  const auto indexBufferSize = mIndexCount * sizeof(uint32_t);
  {
    auto result = rhi::Buffer::create({
        .size        = indexBufferSize,
        .usageFlags  = vk::BufferUsageFlagBits::eIndexBuffer,
        .hostVisible = false,
        .label       = "Terrain-IndexBuffer",
        .device      = mVulkanContext->getDevice(),
    });
    exitOnError(result);
    mIndexBuffer = std::move(result.value());
  }

  // Staging buffer
  const auto stagingResult = rhi::Buffer::create({
      .size        = vertexBufferSize + indexBufferSize,
      .hostVisible = true,
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(stagingResult);
  auto staging = std::move(stagingResult.value());

  staging->setData(vertices.data(), vertexBufferSize, 0);
  staging->setData(indices.data(), indexBufferSize, vertexBufferSize);

  // Copy from staging buffer to device
  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    const auto vertexRegion = vk::BufferCopy2().setSrcOffset(0).setDstOffset(0).setSize(vertexBufferSize);
    cb.copyBuffer2(vk::CopyBufferInfo2().setSrcBuffer(staging->getHandle()).setDstBuffer(mVertexBuffer->getHandle()).setRegions(vertexRegion));

    const auto indexRegion = vk::BufferCopy2().setSrcOffset(vertexBufferSize).setDstOffset(0).setSize(indexBufferSize);
    cb.copyBuffer2(vk::CopyBufferInfo2().setSrcBuffer(staging->getHandle()).setDstBuffer(mIndexBuffer->getHandle()).setRegions(indexRegion));
  });
}

void Terrain::loadHeightmap() noexcept
{
  const char* filepath = "assets/textures/heightmap.png";

  int            width, height, channels;
  const stbi_us* pixels = stbi_load_16(filepath, &width, &height, &channels, 1);

  if(!pixels)
  {
    exitWithError("Failed to load heightmap: {}", filepath);
  }

  const auto imageSize = static_cast<vk::DeviceSize>(width) * static_cast<vk::DeviceSize>(height) * sizeof(uint16_t);

  // Create staging buffer
  const auto stagingResult = rhi::Buffer::create({
      .size        = imageSize,
      .hostVisible = true,
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(stagingResult);
  auto staging = std::move(stagingResult.value());

  staging->setData(pixels, imageSize, 0);
  stbi_image_free((void*)pixels);

  // Create the heightmap image
  const auto imageResult = rhi::Image::create({
      .extent     = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
      .usageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      .format     = vk::Format::eR16Unorm,
      .mipmapping = true,
      .label      = "Terrain-Heightmap",
      .device     = mVulkanContext->getDevice(),
  });
  exitOnError(imageResult);
  mHeightmapImage = std::move(imageResult.value());

  // Copy from staging buffer to device
  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    const auto barrier_toDst = vk::ImageMemoryBarrier2()
                                   .setImage(mHeightmapImage->getHandle())
                                   .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                                   .setOldLayout(vk::ImageLayout::eUndefined)
                                   .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                                   .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                                   .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                                   .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
                                   .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);

    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier_toDst));

    // Copy buffer to image
    const auto region = vk::BufferImageCopy2()
                            .setBufferOffset(0)
                            .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                            .setImageExtent({static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1});

    cb.copyBufferToImage2(vk::CopyBufferToImageInfo2()
                              .setSrcBuffer(staging->getHandle())
                              .setDstImage(mHeightmapImage->getHandle())
                              .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
                              .setRegions(region));
  });

  // Generate mipmaps
  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    mHeightmapImage->generateMipmaps(
        cb,
        std::make_optional<rhi::ImageState>({vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits2::eTransferWrite,
                                             vk::PipelineStageFlagBits2::eTransfer}),
        std::make_optional<rhi::ImageState>(
            {vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead,
             vk::PipelineStageFlagBits2::eTessellationEvaluationShader | vk::PipelineStageFlagBits2::eFragmentShader}));
  });

  // Create sampler
  auto samplerInfo = vk::SamplerCreateInfo()
                         .setMagFilter(vk::Filter::eLinear)
                         .setMinFilter(vk::Filter::eLinear)
                         .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                         .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                         .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                         .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                         .setMinLod(0.0f)
                         .setMaxLod(static_cast<float>(mHeightmapImage->getProperties().levelCount))
                         .setBorderColor(vk::BorderColor::eFloatOpaqueWhite);

  mHeightmapSampler = mVulkanContext->getDevice()->getHandle().createSampler(samplerInfo);
}

void Terrain::createTerrainDescriptor() noexcept
{
  // Create tessellation uniform buffer
  const auto uboResult = rhi::Buffer::create({
      .size        = sizeof(TerrainTessellationData),
      .usageFlags  = vk::BufferUsageFlagBits::eUniformBuffer,
      .hostVisible = true,
      .label       = "Terrain-TessellationUBO",
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(uboResult);
  mTessellationUBO = std::move(uboResult.value());

  // Create descriptor for terrain-specific resources
  constexpr vk::ShaderStageFlags tesc_tese_frag = vk::ShaderStageFlagBits::eTessellationControl
                                                  | vk::ShaderStageFlagBits::eTessellationEvaluation
                                                  | vk::ShaderStageFlagBits::eFragment;

  const auto descResult = rhi::Descriptor::create({
      .bindings =
          {
              // Binding 0: Tessellation uniform buffer
              {0, vk::DescriptorType::eUniformBuffer, 1, tesc_tese_frag},
              // Binding 1: Heightmap sampler
              {1, vk::DescriptorType::eCombinedImageSampler, 1, tesc_tese_frag},
          },
      .setCount = mVulkanContext->getSwapchain()->getImageCount(),
      .label    = "TerrainDescriptor",
      .device   = mVulkanContext->getDevice(),
  });
  exitOnError(descResult);
  mDescriptor = std::move(descResult.value());

  // Write descriptor sets
  for(uint32_t i = 0; i < mDescriptor->getSetCount(); i++)
  {
    const auto uboInfo =
        vk::DescriptorBufferInfo().setBuffer(mTessellationUBO->getHandle()).setOffset(0).setRange(sizeof(TerrainTessellationData));

    const auto imageInfo =
        vk::DescriptorImageInfo().setSampler(mHeightmapSampler).setImageView(mHeightmapImage->getImageView()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    const auto write0 = vk::WriteDescriptorSet()
                            .setBufferInfo(uboInfo)
                            .setDstBinding(0)
                            .setDescriptorCount(1)
                            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                            .setDstSet(mDescriptor->getSet(i));

    const auto write1 = vk::WriteDescriptorSet()
                            .setImageInfo(imageInfo)
                            .setDstBinding(1)
                            .setDescriptorCount(1)
                            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                            .setDstSet(mDescriptor->getSet(i));

    std::array writes = {write0, write1};
    mVulkanContext->getDevice()->getHandle().updateDescriptorSets(writes, {});
  }
}

void Terrain::createPipeline() noexcept
{
  using enum vk::ShaderStageFlagBits;

  // Configure state for tessellated terrain
  mPipeline = rhi::GraphicsPipelineBuilder()
                  .addDescriptor(0, mSceneDescriptor)
                  .addDescriptor(1, mDescriptor)
                  .addVertexType<Vertex>()
                  .addShader({"assets/shaders/terrain.vert.glsl", eVertex})
                  .addShader({"assets/shaders/terrain.tesc.glsl", eTessellationControl})
                  .addShader({"assets/shaders/terrain.tese.glsl", eTessellationEvaluation})
                  .addShader({"assets/shaders/terrain.frag.glsl", eFragment})
                  .addAttachment(vk::Format::eB8G8R8A8Unorm)
                  .setDepthFormat(vk::Format::eD32Sfloat)
                  .configure([](rhi::GraphicsPipelineState& state) {
                    // Use patch list topology with 4 control points per patch
                    state.inputAssemblyState.setTopology(vk::PrimitiveTopology::ePatchList);
                    state.tessellationState = vk::PipelineTessellationStateCreateInfo().setPatchControlPoints(4);
                    // Disable face culling so both sides are visible (useful for terrain)
                    state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                  })
                  .setName("TerrainPipeline")
                  .create(mVulkanContext->getDevice());
}

}  // namespace ptvc
