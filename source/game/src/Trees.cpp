#include "Trees.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <physics/Physics.hpp>
#include <core/IO.hpp>
#include <render/GraphicsPipeline.hpp>
#include <scene/Vertex.hpp>
#include <vulkan/render/VertexType.hpp>

namespace {

struct InstanceMatrix
{
  glm::mat4 model;

  [[nodiscard]] static ptvc::rhi::VertexAttributes getAttributes(uint32_t firstLoc, uint32_t binding) noexcept
  {
    return {
        {firstLoc + 0, binding, vk::Format::eR32G32B32A32Sfloat, offsetof(InstanceMatrix, model) + 0},
        {firstLoc + 1, binding, vk::Format::eR32G32B32A32Sfloat, offsetof(InstanceMatrix, model) + 16},
        {firstLoc + 2, binding, vk::Format::eR32G32B32A32Sfloat, offsetof(InstanceMatrix, model) + 32},
        {firstLoc + 3, binding, vk::Format::eR32G32B32A32Sfloat, offsetof(InstanceMatrix, model) + 48},
    };
  }

  [[nodiscard]] static ptvc::rhi::VertexBinding getBinding(uint32_t binding) noexcept
  {
    return {binding, sizeof(InstanceMatrix), vk::VertexInputRate::eInstance};
  }

  [[nodiscard]] static constexpr uint32_t getAttributeCount() noexcept { return 4; }
};

}  // namespace

Trees::Trees(const ptvc::GameObjectParams&      params,
             SPtr<ptvc::Physics>                physics,
             SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
             const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
    : GameObject(params)
    , mVulkanContext(std::move(vulkanContext))
    , mPhysics(std::move(physics))
{
  assert(mVulkanContext && "Trees requires a valid VulkanContext pointer");
  assert(mPhysics && "Trees requires a valid Physics pointer");

  loadFromProps();
  if(mInstances.empty())
    return;

  placeOnTerrain();

  // Deterministic seed so the forest layout is stable between runs.
  std::mt19937                          rng(0xC0FFEEu);
  std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
  std::uniform_real_distribution<float> scaleDist(kTreeMinScale, kTreeMaxScale);

  for(auto& instance : mInstances)
  {
    const float angle = angleDist(rng);
    const float scale = scaleDist(rng);

    const glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 s   = glm::scale(glm::mat4(1.0f), glm::vec3(scale));

    instance.yaw   = angle;
    instance.scale = scale;
    instance.model = glm::translate(glm::mat4(1.0f), instance.position) * yaw * s;
  }

  loadTreeModel();
  createTreeTexture();
  createInstanceBuffer();
  createPipeline(sceneDescriptor);
  createShadowPipeline(sceneDescriptor);
  createPhysicsBodies();
}

Trees::~Trees()
{
  destroyPhysicsBodies();
  if(mTextureSampler)
    mVulkanContext->getDevice()->getHandle().destroySampler(mTextureSampler);
}

void Trees::collectDebugMeshes(std::vector<ptvc::GameObject::DebugMesh>& out) const noexcept
{
  if(!mTree || mInstances.empty())
    return;

  for(const auto& instance : mInstances)
    out.push_back({instance.model, mTree.get()});
}

void Trees::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  if(!mPipeline || !mTree || mInstances.empty())
    return;

  mPipeline->bind(frame, frame.commandBuffer);

  const ptvc::GPUGameObjectData pc = {
      .model              = glm::mat4(1.0f),
      .solidColor         = {1.0f, 1.0f, 1.0f, 1.0f},
      .materialProperties = {0.1f, 0.7f, 0.2f, 10.0f},
      .showFresnel        = 0,
      .useExampleTexture  = 1,
  };
  mPipeline->pushConstant(&pc, frame.commandBuffer);

  constexpr vk::DeviceSize offsets[2] = {0, 0};
  const vk::Buffer         buffers[2] = {
      mTree->getVertexBuffer()->getHandle(),
      mInstanceBuffer->getHandle(),
  };
  frame.commandBuffer.bindVertexBuffers(0, 2, buffers, offsets);
  frame.commandBuffer.bindIndexBuffer(mTree->getIndexBuffer()->getHandle(), 0, vk::IndexType::eUint32);
  frame.commandBuffer.drawIndexed(static_cast<uint32_t>(mTree->getIndices().size()),
                                  static_cast<uint32_t>(mInstances.size()), 0, 0, 0);
}

void Trees::onRenderShadow(const ptvc::rhi::Frame& frame, ptvc::rhi::Pipeline& /*shadowPipeline*/, const glm::mat4& lightVP) noexcept
{
  if(!mTreeShadowPipeline || !mTree || mInstances.empty())
    return;

  mTreeShadowPipeline->bind(frame, frame.commandBuffer);
  mTreeShadowPipeline->pushConstant(&lightVP, frame.commandBuffer);

  constexpr vk::DeviceSize offsets[2] = {0, 0};
  const vk::Buffer         buffers[2] = {
      mTree->getVertexBuffer()->getHandle(),
      mInstanceBuffer->getHandle(),
  };
  frame.commandBuffer.bindVertexBuffers(0, 2, buffers, offsets);
  frame.commandBuffer.bindIndexBuffer(mTree->getIndexBuffer()->getHandle(), 0, vk::IndexType::eUint32);
  frame.commandBuffer.drawIndexed(static_cast<uint32_t>(mTree->getIndices().size()),
                                  static_cast<uint32_t>(mInstances.size()), 0, 0, 0);
}

void Trees::loadFromProps()
{
  constexpr const char* kPropsPath = "assets/textures/props.png";

  int            width = 0, height = 0, channels = 0;
  const stbi_uc* pixels = stbi_load(kPropsPath, &width, &height, &channels, STBI_rgb);

  if(!pixels)
  {
    exitWithError("Failed to load props texture: {}", kPropsPath);
    return;
  }

  std::vector<glm::vec2> treePixels;
  for(int y = 0; y < height; ++y)
  {
    for(int x = 0; x < width; ++x)
    {
      const stbi_uc* p = pixels + (static_cast<ptrdiff_t>(y) * width + x) * 3;
      if(p[0] == 0 && p[1] == 255 && p[2] == 0)
        treePixels.emplace_back(static_cast<float>(x), static_cast<float>(y));
    }
  }

  stbi_image_free((void*)pixels);

  if(treePixels.empty())
  {
    spdlog::warn("props.png contained no tree pixels (R=0, G=255, B=0)");
    return;
  }

  mInstances.clear();
  mInstances.reserve(treePixels.size());

  for(const auto& px : treePixels)
  {
    const float u = px.x / static_cast<float>(width - 1);
    const float v = px.y / static_cast<float>(height - 1);

    const glm::vec3 position(u * kTerrainWorldSize - kTerrainHalfSize, 0.0f, v * kTerrainWorldSize - kTerrainHalfSize);

    TreeInstance instance{};
    instance.position = position;
    instance.model    = glm::translate(glm::mat4(1.0f), position);
    mInstances.push_back(instance);
  }
}

void Trees::placeOnTerrain()
{
  if(mInstances.empty())
    return;

  constexpr const char* kHeightmapPath = "assets/textures/heightmap.png";

  int            width = 0, height = 0, channels = 0;
  const stbi_us* pixels = stbi_load_16(kHeightmapPath, &width, &height, &channels, 1);

  if(!pixels)
  {
    exitWithError("Failed to load heightmap for tree sampling: {}", kHeightmapPath);
    return;
  }

  for(auto& instance : mInstances)
  {
    const glm::vec3 pos = instance.position;

    const float u = (pos.x + kTerrainHalfSize) / kTerrainWorldSize;
    const float v = (pos.z + kTerrainHalfSize) / kTerrainWorldSize;

    int hx = static_cast<int>(std::round(u * static_cast<float>(width - 1)));
    int hy = static_cast<int>(std::round(v * static_cast<float>(height - 1)));
    hx     = std::clamp(hx, 0, width - 1);
    hy     = std::clamp(hy, 0, height - 1);

    const uint16_t raw        = pixels[static_cast<ptrdiff_t>(hy) * width + hx];
    const float    normalized = static_cast<float>(raw) / 65535.0f;

    instance.position.y = normalized * kHeightScale;
    instance.model      = glm::translate(glm::mat4(1.0f), instance.position);
  }

  stbi_image_free((void*)pixels);
}

void Trees::loadTreeModel()
{
  constexpr const char* kTreePath = "assets/models/tree/scene.gltf";

  auto tree = makeUnique<ptvc::glTF>(kTreePath);
  tree->init(mVulkanContext.get());

  if(tree->getVertices().empty())
  {
    spdlog::error("Tree glTF loaded no geometry from '{}'", kTreePath);
    return;
  }

  mTree = std::move(tree);
}

void Trees::createTreeTexture()
{
  constexpr const char* kTexturePath = "assets/models/tree/textures/tree_baseColor.png";

  auto texData = ptvc::io::loadTextureFromFile(kTexturePath, STBI_rgb_alpha);
  if(!texData.pixels)
  {
    exitWithError("Failed to load tree texture: {}", kTexturePath);
    return;
  }

  spdlog::info("Tree texture loaded from disk: {}x{} ({} channels)", texData.width, texData.height, texData.channels);

  const auto imageSize = static_cast<vk::DeviceSize>(texData.width) * static_cast<vk::DeviceSize>(texData.height) * 4;

  auto stagingResult = ptvc::rhi::Buffer::create({
      .size        = imageSize,
      .hostVisible = true,
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(stagingResult);
  auto staging = std::move(stagingResult.value());
  staging->setData(texData.pixels, imageSize, 0);
  texData.free();

  auto imageResult = ptvc::rhi::Image::create({
      .extent     = {static_cast<uint32_t>(texData.width), static_cast<uint32_t>(texData.height)},
      .usageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      .format     = vk::Format::eR8G8B8A8Unorm,
      .mipmapping = true,
      .label      = "Tree-Texture",
      .device     = mVulkanContext->getDevice(),
  });
  exitOnError(imageResult);
  mTexture = std::move(imageResult.value());

  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    const auto barrier = vk::ImageMemoryBarrier2()
                             .setImage(mTexture->getHandle())
                             .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                             .setOldLayout(vk::ImageLayout::eUndefined)
                             .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                             .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                             .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                             .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
                             .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);
    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier));

    const auto region = vk::BufferImageCopy2()
                            .setBufferOffset(0)
                            .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                            .setImageExtent({static_cast<uint32_t>(texData.width), static_cast<uint32_t>(texData.height), 1});
    cb.copyBufferToImage2(vk::CopyBufferToImageInfo2()
                              .setSrcBuffer(staging->getHandle())
                              .setDstImage(mTexture->getHandle())
                              .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
                              .setRegions(region));
  });

  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    mTexture->generateMipmaps(
        cb,
        std::make_optional<ptvc::rhi::ImageState>({vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits2::eTransferWrite,
                                                   vk::PipelineStageFlagBits2::eTransfer}),
        std::make_optional<ptvc::rhi::ImageState>({vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead,
                                                   vk::PipelineStageFlagBits2::eFragmentShader}));
  });

  auto samplerInfo = vk::SamplerCreateInfo()
                         .setMagFilter(vk::Filter::eLinear)
                         .setMinFilter(vk::Filter::eLinear)
                         .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                         .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                         .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                         .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                         .setMinLod(0.0f)
                         .setMaxLod(static_cast<float>(mTexture->getProperties().levelCount));
  mTextureSampler  = mVulkanContext->getDevice()->getHandle().createSampler(samplerInfo);

  constexpr vk::ShaderStageFlags fragStage  = vk::ShaderStageFlagBits::eFragment;
  auto                           descResult = ptvc::rhi::Descriptor::create({
      .bindings =
          {
              {0, vk::DescriptorType::eCombinedImageSampler, 1, fragStage},
          },
      .setCount = mVulkanContext->getSwapchain()->getImageCount(),
      .label    = "Tree-TextureDescriptor",
      .device   = mVulkanContext->getDevice(),
  });
  exitOnError(descResult);
  mTextureDescriptor = std::move(descResult.value());

  for(uint32_t i = 0; i < mTextureDescriptor->getSetCount(); i++)
  {
    const auto imageInfo =
        vk::DescriptorImageInfo().setSampler(mTextureSampler).setImageView(mTexture->getImageView()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    const auto write = vk::WriteDescriptorSet()
                           .setImageInfo(imageInfo)
                           .setDstBinding(0)
                           .setDescriptorCount(1)
                           .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                           .setDstSet(mTextureDescriptor->getSet(i));
    mVulkanContext->getDevice()->getHandle().updateDescriptorSets(write, {});
  }
}

void Trees::createInstanceBuffer()
{
  std::vector<glm::mat4> instanceMatrices;
  instanceMatrices.reserve(mInstances.size());
  for(const auto& instance : mInstances)
    instanceMatrices.push_back(instance.model);

  const auto size = sizeof(glm::mat4) * instanceMatrices.size();

  auto result = ptvc::rhi::Buffer::create({
      .size        = size,
      .usageFlags  = vk::BufferUsageFlagBits::eVertexBuffer,
      .hostVisible = false,
      .label       = "Tree-InstanceBuffer",
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(result);
  mInstanceBuffer = std::move(result.value());

  auto staging = ptvc::rhi::Buffer::create({
      .size        = size,
      .hostVisible = true,
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(staging);
  staging.value()->setData(instanceMatrices.data(), size, 0);

  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    const auto region = vk::BufferCopy2().setSrcOffset(0).setDstOffset(0).setSize(size);
    cb.copyBuffer2(
        vk::CopyBufferInfo2().setSrcBuffer(staging.value()->getHandle()).setDstBuffer(mInstanceBuffer->getHandle()).setRegions(region));
  });
}

void Trees::createPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
{
  using enum vk::ShaderStageFlagBits;

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, sceneDescriptor)
                      .addDescriptor(1, mTextureDescriptor)
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(ptvc::GPUGameObjectData)})
                      .addVertexType<ptvc::Vertex>(0)
                      .addVertexType<InstanceMatrix>(1)
                      .addShader({"assets/shaders/tree.vert.glsl", eVertex})
                      .addShader({"assets/shaders/phong.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                        state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                      })
                      .setName("TreePipeline")
                      .create(mVulkanContext->getDevice());

  mPipeline = std::move(pipeline);
}

void Trees::createShadowPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
{
  using enum vk::ShaderStageFlagBits;

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, sceneDescriptor)
                      .addVertexType<ptvc::Vertex>(0)
                      .addVertexType<InstanceMatrix>(1)
                      .addPushConstantRange({eVertex, 0, sizeof(glm::mat4)})
                      .addShader({"assets/shaders/tree_shadow.vert.glsl", eVertex})
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                        state.rasterizationState.setCullMode(vk::CullModeFlagBits::eBack);
                        state.rasterizationState.setDepthBiasEnable(true);
                        state.rasterizationState.setDepthBiasConstantFactor(0.5f);
                        state.rasterizationState.setDepthBiasSlopeFactor(1.5f);
                        state.rasterizationState.setDepthBiasClamp(0.0f);
                      })
                      .setName("TreeShadowPipeline")
                      .create(mVulkanContext->getDevice());

  mTreeShadowPipeline = std::move(pipeline);
}

void Trees::createPhysicsBodies()
{
  if(!mPhysics || mInstances.empty())
    return;

  auto& bodyInterface = mPhysics->getBodyInterface();
  mTrunkBodyIds.reserve(mInstances.size());

  for(const auto& instance : mInstances)
  {
    const float hw = kTrunkHalfWidth * instance.scale;
    const float hh = kTrunkHalfHeight * instance.scale;
    const float cy = kTrunkCenterY * instance.scale;

    JPH::BoxShapeSettings boxSettings(JPH::Vec3(hw, hh, hw));
    auto                  shapeResult = boxSettings.Create();
    if(!shapeResult.IsValid())
    {
      spdlog::error("Trees: failed to create trunk BoxShape: {}", shapeResult.GetError());
      mTrunkBodyIds.push_back(JPH::BodyID());
      continue;
    }

    const JPH::RVec3 center(instance.position.x, instance.position.y + cy, instance.position.z);
    const JPH::Quat  rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), instance.yaw);

    JPH::BodyCreationSettings bodySettings(shapeResult.Get(), center, rotation, JPH::EMotionType::Static,
                                           ptvc::Physics::ObjectLayer::Static);

    auto* body = bodyInterface.CreateBody(bodySettings);
    if(!body)
    {
      spdlog::warn("Trees: failed to allocate trunk physics body (body limit reached?)");
      mTrunkBodyIds.push_back(JPH::BodyID());
      continue;
    }

    bodyInterface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
    mTrunkBodyIds.push_back(body->GetID());
  }
}

void Trees::destroyPhysicsBodies()
{
  if(!mPhysics)
    return;

  auto& bodyInterface = mPhysics->getBodyInterface();
  for(const auto& id : mTrunkBodyIds)
  {
    if(!id.IsInvalid())
    {
      bodyInterface.RemoveBody(id);
      bodyInterface.DestroyBody(id);
    }
  }
  mTrunkBodyIds.clear();
}
