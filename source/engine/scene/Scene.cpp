#include "Scene.hpp"

#include "Lights.hpp"

namespace ptvc {
Scene::Scene(const SPtr<rhi::VulkanContext>& vulkanContext)
    : mVulkanContext(vulkanContext)
{
  createSceneDescriptor();
}

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
      .color     = glm::vec4(0.85f, 0.85f, 0.85f, 0.0f),
      .direction = glm::vec4(0.0f, 1.0f, -1.0f, 0.0f),
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

  constexpr vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAllGraphics;

  mDescriptor = rhi::Descriptor::create(
                    {
                        .bindings =
                            {
                                {SceneDescriptorBindings_CameraUniform, vk::DescriptorType::eUniformBuffer, 1, stages},
                                {SceneDescriptorBindings_DirectionalLight, vk::DescriptorType::eUniformBuffer, 1, stages},
                                {SceneDescriptorBindings_PointLight, vk::DescriptorType::eUniformBuffer, 1, stages},
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
}  // namespace ptvc
