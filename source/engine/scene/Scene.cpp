#include "Scene.hpp"

#include <scene/Lights.hpp>
#include <scene/ShadowMap.hpp>

namespace ptvc {
Scene::Scene(const SPtr<rhi::VulkanContext>& vulkanContext)
    : mVulkanContext(vulkanContext)
{
  createSceneDescriptor();

  // Create the shadow map after the descriptor is ready
  mShadowMap = makeUnique<ShadowMap>(mVulkanContext, mDescriptor);
  writeShadowDescriptor();
}

Scene::~Scene() = default;

void Scene::onEvent(const SDL_Event& event) noexcept
{
  if(mCamera)
  {
    mCamera->onEvent(event);
  }
  for(const auto& object : mObjects)
  {
    object->onEvent(event);
  }
}

void Scene::onUpdate(const float deltaTime, const rhi::Frame& frame) noexcept
{
  if(mCamera)
  {
    mCamera->onUpdate(deltaTime);

    const auto [w, h] = mVulkanContext->getSwapchain()->getExtent();
    const auto aspect = static_cast<float>(w) / static_cast<float>(h);
    const auto data   = mCamera->getCameraData(aspect);

    mCameraUniformBuffers[frame.currentFrameIndex]->setData(&data, sizeof(CameraData), 0);

    // Update shadow map light-space matrix
    constexpr auto sunLight = DirectionalLight{
        .color     = glm::vec4(0.95f, 0.9f, 0.8f, 0.0f),
        .direction = glm::vec4(0.6f, -0.4f, 0.4f, 0.0f),
    };
    mShadowMap->updateLightSpace(data, sunLight);
  }
  for(const auto& object : mObjects)
  {
    object->onUpdate(deltaTime, frame);
  }
}

const std::vector<UPtr<GameObject>>& Scene::getGameObjects() const noexcept
{
  return mObjects;
}

const SPtr<rhi::Descriptor>& Scene::getDescriptor() const noexcept
{
  return mDescriptor;
}

ICamera& Scene::getCamera() noexcept
{
  return *mCamera;
}

void Scene::renderShadowPass(const rhi::Frame& frame) noexcept
{
  if(mShadowMap)
    mShadowMap->renderShadowPass(frame, *this);
}

void Scene::createSceneDescriptor() noexcept
{
  for(auto i = 0; i < mVulkanContext->getSwapchain()->getImageCount(); i++)
  {
    mCameraUniformBuffers.push_back(rhi::Buffer::create({
                                                            .size        = sizeof(CameraData),
                                                            .usageFlags  = vk::BufferUsageFlagBits::eUniformBuffer,
                                                            .hostVisible = true,
                                                            .label       = std::format("CameraUniformBuffer[i={}]", i),
                                                            .device      = mVulkanContext->getDevice(),
                                                        })
                                        .value());
  }

  mDirectionalLight = rhi::Buffer::create({
                                              .size        = sizeof(DirectionalLight),
                                              .usageFlags  = vk::BufferUsageFlagBits::eUniformBuffer,
                                              .hostVisible = true,
                                              .label       = "DirectionalLight",
                                              .device      = mVulkanContext->getDevice(),
                                          })
                          .value();

  constexpr auto dirLight = DirectionalLight{
      .color     = glm::vec4(0.95f, 0.9f, 0.8f, 0.0f),
      .direction = glm::vec4(0.6f, -0.4f, 0.4f, 0.0f),
  };
  mDirectionalLight->setData(&dirLight, sizeof(DirectionalLight), 0);

  mPointLight = rhi::Buffer::create({
                                        .size        = sizeof(PointLight),
                                        .usageFlags  = vk::BufferUsageFlagBits::eUniformBuffer,
                                        .hostVisible = true,
                                        .label       = "PointLight",
                                        .device      = mVulkanContext->getDevice(),
                                    })
                    .value();

  constexpr auto pointLight = PointLight{
      .color       = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
      .position    = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
      .attenuation = glm::vec4(1.0f, 0.4f, 0.1f, 0.0f),
  };
  mPointLight->setData(&pointLight, sizeof(PointLight), 0);

  constexpr vk::ShaderStageFlags stages   = vk::ShaderStageFlagBits::eAllGraphics;
  constexpr vk::ShaderStageFlags fragOnly = vk::ShaderStageFlagBits::eFragment;

  mDescriptor = rhi::Descriptor::create(
                    {
                        .bindings =
                            {
                                {SceneDescriptorBindings_CameraUniform, vk::DescriptorType::eUniformBuffer, 1, stages},
                                {SceneDescriptorBindings_DirectionalLight, vk::DescriptorType::eUniformBuffer, 1, stages},
                                {SceneDescriptorBindings_PointLight, vk::DescriptorType::eUniformBuffer, 1, stages},
                                {SceneDescriptorBindings_ShadowMap, vk::DescriptorType::eCombinedImageSampler, 1, fragOnly},
                                {SceneDescriptorBindings_LightSpace, vk::DescriptorType::eUniformBuffer, 1, stages},
                            },
                        .setCount = mVulkanContext->getSwapchain()->getImageCount(),
                        .label    = "SceneDescriptor",
                        .device   = mVulkanContext->getDevice(),
                    })
                    .value();

  for(auto i = 0; i < mDescriptor->getSetCount(); i++)
  {
    const auto bufferInfo = vk::DescriptorBufferInfo()
                                .setBuffer(mCameraUniformBuffers[i]->getHandle())
                                .setOffset(0)
                                .setRange(mCameraUniformBuffers[i]->getSize());
    const auto write0 = vk::WriteDescriptorSet()
                            .setBufferInfo(bufferInfo)
                            .setDstBinding(SceneDescriptorBindings_CameraUniform)
                            .setDescriptorCount(1)
                            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                            .setDstSet(mDescriptor->getSet(i));

    const auto dirBufferInfo =
        vk::DescriptorBufferInfo().setBuffer(mDirectionalLight->getHandle()).setOffset(0).setRange(mDirectionalLight->getSize());
    const auto write1 = vk::WriteDescriptorSet()
                            .setBufferInfo(dirBufferInfo)
                            .setDstBinding(SceneDescriptorBindings_DirectionalLight)
                            .setDescriptorCount(1)
                            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                            .setDstSet(mDescriptor->getSet(i));

    const auto pointBufferInfo =
        vk::DescriptorBufferInfo().setBuffer(mPointLight->getHandle()).setOffset(0).setRange(mPointLight->getSize());
    const auto write2 = vk::WriteDescriptorSet()
                            .setBufferInfo(pointBufferInfo)
                            .setDstBinding(SceneDescriptorBindings_PointLight)
                            .setDescriptorCount(1)
                            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                            .setDstSet(mDescriptor->getSet(i));

    std::array writes = {write0, write1, write2};

    mVulkanContext->getDevice()->getHandle().updateDescriptorSets(writes, {});
  }
}

void Scene::writeShadowDescriptor() noexcept
{
  if(!mShadowMap)
    return;

  const auto lightSpaceUBO = mShadowMap->getLightSpaceUBO();

  for(uint32_t i = 0; i < mDescriptor->getSetCount(); i++)
  {
    const auto shadowInfo = vk::DescriptorImageInfo()
                                .setSampler(mShadowMap->getShadowMapSampler())
                                .setImageView(mShadowMap->getShadowMapView())
                                .setImageLayout(vk::ImageLayout::eDepthReadOnlyOptimal);

    const auto write3 = vk::WriteDescriptorSet()
                            .setImageInfo(shadowInfo)
                            .setDstBinding(SceneDescriptorBindings_ShadowMap)
                            .setDescriptorCount(1)
                            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                            .setDstSet(mDescriptor->getSet(i));

    const auto lightSpaceBufferInfo =
        vk::DescriptorBufferInfo().setBuffer(lightSpaceUBO->getHandle()).setOffset(0).setRange(lightSpaceUBO->getSize());

    const auto write4 = vk::WriteDescriptorSet()
                            .setBufferInfo(lightSpaceBufferInfo)
                            .setDstBinding(SceneDescriptorBindings_LightSpace)
                            .setDescriptorCount(1)
                            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                            .setDstSet(mDescriptor->getSet(i));

    std::array writes = {write3, write4};
    mVulkanContext->getDevice()->getHandle().updateDescriptorSets(writes, {});
  }
}

}  // namespace ptvc
