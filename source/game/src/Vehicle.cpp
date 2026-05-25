#include "Vehicle.hpp"

#include <physics/Physics.hpp>
#include <vulkan/render/GraphicsPipeline.hpp>
#include <scene/primitives/Cube.hpp>
#include <scene/Vertex.hpp>

#include <spdlog/spdlog.h>

Vehicle::Vehicle(const ptvc::GameObjectParams&      params,
                 SPtr<ptvc::Physics>                physics,
                 SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
                 const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
    : GameObject(params)
    , mPhysics(std::move(physics))
    , mVulkanContext(std::move(vulkanContext))
{
  assert(mPhysics && "Vehicle requires a valid Physics pointer");
  assert(mVulkanContext && "Vehicle requires a valid VulkanContext pointer");

  createDummyTextureDescriptor();
  createRenderResources(sceneDescriptor);

  // Create the physics simulation
  mVehiclePhysics  = makeUnique<ptvc::VehiclePhysics>(*mPhysics, glm::vec3(-15.0f, 30.0f, -15.0f));
  mTransform.scale = {ptvc::VehiclePhysics::HALF_WIDTH * 2.0f, ptvc::VehiclePhysics::HALF_HEIGHT * 2.0f,
                      ptvc::VehiclePhysics::HALF_LENGTH * 2.0f};
}

Vehicle::~Vehicle()
{
  if(mDummySampler)
  {
    mVulkanContext->getDevice()->getHandle().destroySampler(mDummySampler);
  }
}

void Vehicle::onEvent(const SDL_Event& event) noexcept
{
  if(event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
  {
    switch(event.key.key)
    {
      case SDLK_UP:
        mForward = true;
        break;
      case SDLK_DOWN:
        mBackward = true;
        break;
      case SDLK_LEFT:
        mLeft = true;
        break;
      case SDLK_RIGHT:
        mRight = true;
        break;
      default:
        break;
    }
  }
  else if(event.type == SDL_EVENT_KEY_UP)
  {
    switch(event.key.key)
    {
      case SDLK_UP:
        mForward = false;
        break;
      case SDLK_DOWN:
        mBackward = false;
        break;
      case SDLK_LEFT:
        mLeft = false;
        break;
      case SDLK_RIGHT:
        mRight = false;
        break;
      default:
        break;
    }
  }
}

void Vehicle::onUpdate(float /*dt*/, const ptvc::rhi::Frame& /*frame*/) noexcept
{
  if(!mVehiclePhysics)
    return;

  // Compute steering and speed from keyboard state
  float steeringAngle = 0.0f;
  float speed         = 0.0f;

  if(mLeft)
    steeringAngle = kMaxSteeringAngle;
  if(mRight)
    steeringAngle = -kMaxSteeringAngle;
  if(mForward)
    speed = kMaxRotationSpeed;
  if(mBackward)
    speed = -kMaxRotationSpeed;

  mVehiclePhysics->applyInput(steeringAngle, speed);
  mVehiclePhysics->getTransform(mTransform.translate, mTransform.rotation);
}

glm::vec3 Vehicle::getPosition() const
{
  if(mVehiclePhysics)
    return mVehiclePhysics->getPosition();
  return mTransform.translate;
}

void Vehicle::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  const ptvc::GPUGameObjectData pushConstant = {
      .model              = mTransform.getModel(),
      .solidColor         = {1.0f, 0.25f, 0.05f, 1.0f},
      .materialProperties = {0.1f, 0.7f, 0.2f, 10.0f},
      .showFresnel        = 0,
      .useExampleTexture  = 0,
  };

  mPipeline->bind(frame, frame.commandBuffer);
  mPipeline->pushConstant(&pushConstant, frame.commandBuffer);

  mGeometry->draw(frame.commandBuffer);
}

void Vehicle::createDummyTextureDescriptor()
{
  const uint32_t whitePixel = 0xFFFFFFFF;

  // Staging buffer
  auto stagingResult = ptvc::rhi::Buffer::create({
      .size        = sizeof(whitePixel),
      .hostVisible = true,
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(stagingResult);
  auto staging = std::move(stagingResult.value());
  staging->setData(&whitePixel, sizeof(whitePixel), 0);

  // Dummy image
  auto imageResult = ptvc::rhi::Image::create({
      .extent     = {1, 1},
      .usageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      .format     = vk::Format::eR8G8B8A8Unorm,
      .mipmapping = false,
      .label      = "Vehicle-DummyTexture",
      .device     = mVulkanContext->getDevice(),
  });
  exitOnError(imageResult);
  mDummyTexture = std::move(imageResult.value());

  // Upload
  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    const auto barrier = vk::ImageMemoryBarrier2()
                             .setImage(mDummyTexture->getHandle())
                             .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                             .setOldLayout(vk::ImageLayout::eUndefined)
                             .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                             .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                             .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                             .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
                             .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);
    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier));

    const auto region =
        vk::BufferImageCopy2().setBufferOffset(0).setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1}).setImageExtent({1, 1, 1});
    cb.copyBufferToImage2(vk::CopyBufferToImageInfo2()
                              .setSrcBuffer(staging->getHandle())
                              .setDstImage(mDummyTexture->getHandle())
                              .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
                              .setRegions(region));

    const auto barrier2 = vk::ImageMemoryBarrier2()
                              .setImage(mDummyTexture->getHandle())
                              .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                              .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                              .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
                              .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
                              .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                              .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                              .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader);
    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier2));
  });

  // Sampler
  auto samplerInfo = vk::SamplerCreateInfo()
                         .setMagFilter(vk::Filter::eLinear)
                         .setMinFilter(vk::Filter::eLinear)
                         .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                         .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                         .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                         .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                         .setMinLod(0.0f)
                         .setMaxLod(0.0f);
  mDummySampler = mVulkanContext->getDevice()->getHandle().createSampler(samplerInfo);

  // Descriptor
  constexpr vk::ShaderStageFlags fragStage  = vk::ShaderStageFlagBits::eFragment;
  auto                           descResult = ptvc::rhi::Descriptor::create({
                                .bindings =
          {
              {0, vk::DescriptorType::eCombinedImageSampler, 1, fragStage},
          },
                                .setCount = mVulkanContext->getSwapchain()->getImageCount(),
                                .label    = "Vehicle-DummyTextureDescriptor",
                                .device   = mVulkanContext->getDevice(),
  });
  exitOnError(descResult);
  mDummyTextureDescriptor = std::move(descResult.value());

  for(uint32_t i = 0; i < mDummyTextureDescriptor->getSetCount(); i++)
  {
    const auto imageInfo =
        vk::DescriptorImageInfo().setSampler(mDummySampler).setImageView(mDummyTexture->getImageView()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    const auto write = vk::WriteDescriptorSet()
                           .setImageInfo(imageInfo)
                           .setDstBinding(0)
                           .setDescriptorCount(1)
                           .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                           .setDstSet(mDummyTextureDescriptor->getSet(i));
    mVulkanContext->getDevice()->getHandle().updateDescriptorSets(write, {});
  }
}

void Vehicle::createRenderResources(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
{
  using enum vk::ShaderStageFlagBits;

  // Create the pipeline with phong shaders
  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, sceneDescriptor)
                      .addDescriptor(1, mDummyTextureDescriptor)
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(ptvc::GPUGameObjectData)})
                      .addVertexType<ptvc::Vertex>()
                      .addShader({"assets/shaders/phong.vert.glsl", eVertex})
                      .addShader({"assets/shaders/phong.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .setName("VehiclePipeline")
                      .create(mVulkanContext->getDevice());

  auto cube = makeUnique<ptvc::Cube>(1.0f);
  cube->init(mVulkanContext.get());

  mPipeline = std::move(pipeline);
  mGeometry = std::move(cube);
}
