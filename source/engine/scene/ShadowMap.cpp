#include "ShadowMap.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <scene/Lights.hpp>
#include <scene/Scene.hpp>
#include <scene/Terrain.hpp>
#include <scene/GameObject.hpp>
#include <scene/ICamera.hpp>
#include <render/GraphicsPipeline.hpp>

namespace ptvc {

ShadowMap::ShadowMap(const SPtr<rhi::VulkanContext>& vulkanContext, const SPtr<rhi::Descriptor>& sceneDescriptor)
    : mVulkanContext(vulkanContext)
    , mSceneDescriptor(sceneDescriptor)
{
  createShadowResources();
  createObjectShadowPipeline();
}

ShadowMap::~ShadowMap()
{
  if(mShadowMapSampler)
  {
    mVulkanContext->getDevice()->getHandle().destroySampler(mShadowMapSampler);
  }
}

void ShadowMap::updateLightSpace(const CameraData& cameraData, const DirectionalLight& sun) noexcept
{
  const glm::vec3 lightDir = glm::normalize(glm::vec3(sun.direction));
  glm::vec3       up       = glm::vec3(0.0f, 1.0f, 0.0f);
  if(glm::abs(glm::dot(lightDir, up)) > 0.99f)
    up = glm::vec3(0.0f, 0.0f, 1.0f);

  const glm::mat4          invViewProj = glm::inverse(cameraData.proj * cameraData.view);
  std::array<glm::vec4, 8> frustumCorners;
  for(int x = 0; x < 2; ++x)
  {
    for(int y = 0; y < 2; ++y)
    {
      for(int z = 0; z < 2; ++z)
      {
        glm::vec4 corner = invViewProj * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, static_cast<float>(z), 1.0f);
        frustumCorners[x * 4 + y * 2 + z] = corner / corner.w;
      }
    }
  }

  // Define view-space split distances for 3 cascades
  const float splits[4] = {cameraData.nearPlane, 20.0f, 70.0f, 250.0f};

  LightSpaceData data;
  data.cascadeSplits = glm::vec4(splits[1], splits[2], splits[3], 0.0f);

  for(int cascade = 0; cascade < 3; ++cascade)
  {
    // Linear interpolation factor in view-space Z range
    float tNear = (splits[cascade] - cameraData.nearPlane) / (cameraData.farPlane - cameraData.nearPlane);
    float tFar  = (splits[cascade + 1] - cameraData.nearPlane) / (cameraData.farPlane - cameraData.nearPlane);

    // Interpolate corners to get sub-frustum corners
    std::array<glm::vec4, 8> subFrustumCorners;
    for(int x = 0; x < 2; ++x)
    {
      for(int y = 0; y < 2; ++y)
      {
        int nearIndex                = x * 4 + y * 2;
        int farIndex                 = nearIndex + 1;
        subFrustumCorners[nearIndex] = glm::mix(frustumCorners[nearIndex], frustumCorners[farIndex], tNear);
        subFrustumCorners[farIndex]  = glm::mix(frustumCorners[nearIndex], frustumCorners[farIndex], tFar);
      }
    }

    // Sub-frustum center
    glm::vec3 subCenter(0.0f);
    for(const auto& corner : subFrustumCorners)
    {
      subCenter += glm::vec3(corner);
    }
    subCenter /= static_cast<float>(subFrustumCorners.size());

    // Compute light view matrix
    const float     farDistance = cameraData.farPlane * 0.5f;
    const glm::vec3 lightPos    = subCenter - lightDir * farDistance;
    const glm::mat4 lightView   = glm::lookAt(lightPos, subCenter, up);

    // Transform sub-frustum corners to light space and compute AABB
    glm::vec3 minCorner(std::numeric_limits<float>::max());
    glm::vec3 maxCorner(std::numeric_limits<float>::lowest());
    for(const auto& corner : subFrustumCorners)
    {
      const glm::vec4 lightSpaceCorner = lightView * corner;
      minCorner                        = glm::min(minCorner, glm::vec3(lightSpaceCorner));
      maxCorner                        = glm::max(maxCorner, glm::vec3(lightSpaceCorner));
    }

    // Pad X and Y bounds slightly to prevent edge clipping due to tessellation and float precision
    float width  = maxCorner.x - minCorner.x;
    float height = maxCorner.y - minCorner.y;
    minCorner.x -= width * 0.05f;
    maxCorner.x += width * 0.05f;
    minCorner.y -= height * 0.05f;
    maxCorner.y += height * 0.05f;

    // Extend Z range to capture occluders outside the sub-frustum (especially towards the light)
    constexpr float zExtendNear = 400.0f;  // Extend near plane towards light to capture distant shadow casters
    constexpr float zExtendFar  = 100.0f;  // Extend far plane slightly
    minCorner.z -= zExtendFar;
    maxCorner.z += zExtendNear;

    // Build the orthographic projection matrix and invert Y axis
    glm::mat4 lightProj = glm::ortho(minCorner.x, maxCorner.x, minCorner.y, maxCorner.y, -maxCorner.z, -minCorner.z);
    lightProj           = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f)) * lightProj;

    mCascadeLightVPs[cascade] = lightProj * lightView;
    data.lightVP[cascade]     = mCascadeLightVPs[cascade];
  }

  mLightSpaceUBO->setData(&data, sizeof(LightSpaceData), 0);
}

void ShadowMap::renderShadowPass(const rhi::Frame& frame, Scene& scene) noexcept
{
  {
    const auto barrier =
        vk::ImageMemoryBarrier2()
            .setImage(mShadowMapImage->getHandle())
            .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1})
            .setOldLayout(mFirstShadowPass ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthReadOnlyOptimal)
            .setSrcAccessMask(mFirstShadowPass ? vk::AccessFlagBits2::eNone : vk::AccessFlagBits2::eShaderRead)
            .setSrcStageMask(mFirstShadowPass ? vk::PipelineStageFlagBits2::eNone : vk::PipelineStageFlagBits2::eFragmentShader)
            .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests);

    frame.commandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier));
  }

  {
    const auto depthAttachment = vk::RenderingAttachmentInfo()
                                     .setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}))
                                     .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                                     .setImageView(mShadowMapImage->getImageView())
                                     .setLoadOp(vk::AttachmentLoadOp::eClear)
                                     .setStoreOp(vk::AttachmentStoreOp::eStore);

    const auto renderingInfo =
        vk::RenderingInfo().setPDepthAttachment(&depthAttachment).setLayerCount(1).setRenderArea(vk::Rect2D{{0, 0}, mSize});

    frame.commandBuffer.beginRendering(renderingInfo);

    // Loop through the three cascades and render each one
    for(int cascade = 0; cascade < 3; ++cascade)
    {
      // Viewport and scissor for this cascade's portion of the shadow map
      const vk::Viewport shadowViewport = vk::Viewport()
                                              .setX(static_cast<float>(cascade * 2048))
                                              .setY(0.0f)
                                              .setWidth(2048.0f)
                                              .setHeight(2048.0f)
                                              .setMinDepth(0.0f)
                                              .setMaxDepth(1.0f);

      const vk::Rect2D shadowScissor = vk::Rect2D({static_cast<int32_t>(cascade * 2048), 0}, {2048, 2048});

      frame.commandBuffer.setViewport(0, shadowViewport);
      frame.commandBuffer.setScissor(0, shadowScissor);

      // Render terrain
      if(auto* terrain = scene.getTerrain())
      {
        if(mTerrainShadowPipeline && terrain->getVertexBuffer() && terrain->getIndexBuffer())
        {
          mTerrainShadowPipeline->bind(frame, frame.commandBuffer);
          mTerrainShadowPipeline->pushConstant(&mCascadeLightVPs[cascade], frame.commandBuffer);

          const vk::DeviceSize offsets[1] = {0};
          frame.commandBuffer.bindVertexBuffers(0, 1, &terrain->getVertexBuffer()->getHandle(), offsets);
          frame.commandBuffer.bindIndexBuffer(terrain->getIndexBuffer()->getHandle(), 0, vk::IndexType::eUint32);
          frame.commandBuffer.drawIndexed(terrain->getIndexCount(), 1, 0, 0, 0);
        }
      }

      for(const auto& object : scene.getGameObjects())
        object->onRenderShadow(frame, *mObjectShadowPipeline, mCascadeLightVPs[cascade]);
    }

    frame.commandBuffer.endRendering();
  }

  {
    const auto barrier = vk::ImageMemoryBarrier2()
                             .setImage(mShadowMapImage->getHandle())
                             .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1})
                             .setOldLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                             .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
                             .setSrcStageMask(vk::PipelineStageFlagBits2::eLateFragmentTests)
                             .setNewLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
                             .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                             .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader);

    frame.commandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier));
  }

  if(mFirstShadowPass)
    mFirstShadowPass = false;
}

void ShadowMap::createTerrainShadowPipeline(const SPtr<rhi::Descriptor>& terrainDescriptor) noexcept
{
  using enum vk::ShaderStageFlagBits;

  mTerrainShadowPipeline = rhi::GraphicsPipelineBuilder()
                               .addDescriptor(0, mSceneDescriptor)
                               .addDescriptor(1, terrainDescriptor)
                               .addVertexType<Vertex>()
                               .addPushConstantRange({eTessellationEvaluation, 0, sizeof(glm::mat4)})
                               .addShader({"assets/shaders/shadow_terrain.vert.glsl", eVertex})
                               .addShader({"assets/shaders/terrain.tesc.glsl", eTessellationControl})
                               .addShader({"assets/shaders/shadow_terrain.tese.glsl", eTessellationEvaluation})
                               .setDepthFormat(vk::Format::eD32Sfloat)
                               .configure([](rhi::GraphicsPipelineState& state) {
                                 state.inputAssemblyState.setTopology(vk::PrimitiveTopology::ePatchList);
                                 state.tessellationState = vk::PipelineTessellationStateCreateInfo().setPatchControlPoints(4);
                                 state.rasterizationState.setCullMode(vk::CullModeFlagBits::eBack);
                                 state.rasterizationState.setDepthBiasEnable(true);
                                 state.rasterizationState.setDepthBiasConstantFactor(0.5f);
                                 state.rasterizationState.setDepthBiasSlopeFactor(1.5f);
                                 state.rasterizationState.setDepthBiasClamp(0.0f);
                               })
                               .setName("TerrainShadowPipeline")
                               .create(mVulkanContext->getDevice());
}

vk::ImageView ShadowMap::getShadowMapView() const noexcept
{
  return mShadowMapImage->getImageView();
}

vk::Sampler ShadowMap::getShadowMapSampler() const noexcept
{
  return mShadowMapSampler;
}

SPtr<rhi::Buffer> ShadowMap::getLightSpaceUBO() const noexcept
{
  return mLightSpaceUBO;
}

void ShadowMap::createShadowResources() noexcept
{
  // Shadow map depth image
  {
    const auto result = rhi::Image::create({
        .extent     = mSize,
        .usageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        .format     = vk::Format::eD32Sfloat,
        .mipmapping = false,
        .samples    = vk::SampleCountFlagBits::e1,
        .label      = "ShadowMap-DepthImage",
        .device     = mVulkanContext->getDevice(),
    });
    exitOnError(result);
    mShadowMapImage = std::move(result.value());
  }

  // Shadow map sampler
  {
    auto samplerInfo = vk::SamplerCreateInfo()
                           .setMagFilter(vk::Filter::eLinear)
                           .setMinFilter(vk::Filter::eLinear)
                           .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                           .setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
                           .setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
                           .setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
                           .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                           .setCompareEnable(VK_TRUE)
                           .setCompareOp(vk::CompareOp::eLessOrEqual)
                           .setMinLod(0.0f)
                           .setMaxLod(0.0f)
                           .setAnisotropyEnable(VK_FALSE);

    mShadowMapSampler = mVulkanContext->getDevice()->getHandle().createSampler(samplerInfo);
  }

  // Light-space uniform buffer
  {
    const auto result = rhi::Buffer::create({
        .size        = sizeof(LightSpaceData),
        .usageFlags  = vk::BufferUsageFlagBits::eUniformBuffer,
        .hostVisible = true,
        .label       = "ShadowMap-LightSpaceUBO",
        .device      = mVulkanContext->getDevice(),
    });
    exitOnError(result);
    mLightSpaceUBO = std::move(result.value());

    // Initialize with identity
    const LightSpaceData initData = {{glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)}, glm::vec4(0.0f)};
    mLightSpaceUBO->setData(&initData, sizeof(LightSpaceData), 0);
  }
}

void ShadowMap::createObjectShadowPipeline() noexcept
{
  using enum vk::ShaderStageFlagBits;

  mObjectShadowPipeline = rhi::GraphicsPipelineBuilder()
                              .addDescriptor(0, mSceneDescriptor)
                              .addVertexType<Vertex>()
                              .addPushConstantRange({eVertex, 0, sizeof(glm::mat4) + sizeof(glm::mat4)})
                              .addShader({"assets/shaders/shadow.vert.glsl", eVertex})
                              .setDepthFormat(vk::Format::eD32Sfloat)
                              .configure([](rhi::GraphicsPipelineState& state) {
                                state.rasterizationState.setCullMode(vk::CullModeFlagBits::eBack);
                                state.rasterizationState.setDepthBiasEnable(true);
                                state.rasterizationState.setDepthBiasConstantFactor(0.5f);
                                state.rasterizationState.setDepthBiasSlopeFactor(1.5f);
                                state.rasterizationState.setDepthBiasClamp(0.0f);
                              })
                              .setName("ObjectShadowPipeline")
                              .create(mVulkanContext->getDevice());
}

}  // namespace ptvc
