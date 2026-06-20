#include "Vehicle.hpp"

#include <core/IO.hpp>
#include <physics/Physics.hpp>
#include <vulkan/render/GraphicsPipeline.hpp>
#include <scene/glTF.hpp>
#include <scene/Vertex.hpp>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <spdlog/spdlog.h>

namespace {
struct WheelDef
{
  const char*  node;
  ptvc::EWheel physics;
};

constexpr WheelDef kWheels[ptvc::VehiclePhysics::kWheelCount] = {
    {"wheel_FR", ptvc::EWheel::LeftFront},
    {"wheel_FL", ptvc::EWheel::RightFront},
    {"wheel_RR", ptvc::EWheel::LeftRear},
    {"wheel_RL", ptvc::EWheel::RightRear},
};
}  // namespace

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

  createTextureDescriptor();
  createRenderResources(sceneDescriptor);

  mVehiclePhysics = makeUnique<ptvc::VehiclePhysics>(*mPhysics, glm::vec3(-7.5f, 12.5f, -70.0f));
}

Vehicle::~Vehicle()
{
  if(mTextureSampler)
  {
    mVulkanContext->getDevice()->getHandle().destroySampler(mTextureSampler);
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

void Vehicle::collectDebugMeshes(std::vector<ptvc::GameObject::DebugMesh>& out) const noexcept
{
  // Reconstruct the same chassis model matrix used by onRender so the debug
  // view matches the in-game size (kModelScale + chassis-center shift).
  const glm::vec3  shift    = kModelChassisCenter * kModelScale;
  const glm::mat4  carModel = glm::translate(glm::mat4(1.0f), mTransform.translate) * glm::toMat4(mTransform.rotation)
                              * glm::translate(glm::mat4(1.0f), -shift) * glm::scale(glm::mat4(1.0f), glm::vec3(kModelScale));

  if(mGeometry)
    out.push_back({carModel, mGeometry.get()});

  for(int i = 0; i < kWheelCount; ++i)
  {
    if(mWheelGeometry[i])
      out.push_back({wheelModelMatrix(i), mWheelGeometry[i].get()});
  }
}

void Vehicle::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  const glm::vec3 shift = kModelChassisCenter * kModelScale;
  const glm::mat4 carModel = glm::translate(glm::mat4(1.0f), mTransform.translate) * glm::toMat4(mTransform.rotation)
                             * glm::translate(glm::mat4(1.0f), -shift) * glm::scale(glm::mat4(1.0f), glm::vec3(kModelScale));

  const ptvc::GPUGameObjectData bodyPC = {
      .model              = carModel,
      .solidColor         = {1.0f, 0.25f, 0.05f, 1.0f},
      .materialProperties = {0.1f, 0.7f, 0.2f, 10.0f},
      .showFresnel        = 0,
      .useExampleTexture  = 1,
  };

  mPipeline->bind(frame, frame.commandBuffer);
  mPipeline->pushConstant(&bodyPC, frame.commandBuffer);
  mGeometry->draw(frame.commandBuffer);

  for(int i = 0; i < kWheelCount; ++i)
  {
    if(!mWheelGeometry[i])
      continue;

    const ptvc::GPUGameObjectData wheelPC = {
        .model              = wheelModelMatrix(i),
        .solidColor         = {1.0f, 0.25f, 0.05f, 1.0f},
        .materialProperties = {0.1f, 0.7f, 0.2f, 10.0f},
        .showFresnel        = 0,
        .useExampleTexture  = 1,
    };

    mPipeline->pushConstant(&wheelPC, frame.commandBuffer);
    mWheelGeometry[i]->draw(frame.commandBuffer);
  }
}

void Vehicle::onRenderShadow(const ptvc::rhi::Frame& frame, ptvc::rhi::Pipeline& shadowPipeline, const glm::mat4& lightVP) noexcept
{
  const glm::vec3 shift = kModelChassisCenter * kModelScale;
  const glm::mat4 carModel = glm::translate(glm::mat4(1.0f), mTransform.translate) * glm::toMat4(mTransform.rotation)
                             * glm::translate(glm::mat4(1.0f), -shift) * glm::scale(glm::mat4(1.0f), glm::vec3(kModelScale));

  struct alignas(16) ShadowPushConstants
  {
    glm::mat4 model;
    glm::mat4 lightVP;
  };

  shadowPipeline.bind(frame, frame.commandBuffer);

  if(mGeometry)
  {
    const ShadowPushConstants pc{carModel, lightVP};
    shadowPipeline.pushConstant(&pc, frame.commandBuffer);
    mGeometry->draw(frame.commandBuffer);
  }

  for(int i = 0; i < kWheelCount; ++i)
  {
    if(!mWheelGeometry[i])
      continue;

    const ShadowPushConstants pc{wheelModelMatrix(i), lightVP};
    shadowPipeline.pushConstant(&pc, frame.commandBuffer);
    mWheelGeometry[i]->draw(frame.commandBuffer);
  }
}

glm::mat4 Vehicle::wheelModelMatrix(int wheelIndex) const noexcept
{
  glm::vec3 wpos{};
  glm::quat wrot(1.0f, 0.0f, 0.0f, 0.0f);
  if(mVehiclePhysics)
    mVehiclePhysics->getWheelTransform(kWheels[wheelIndex].physics, wpos, wrot);

  const glm::mat4 Mphys = glm::translate(glm::mat4(1.0f), wpos + glm::vec3(0.0f, 0.15f, 0.0f)) * glm::mat4_cast(wrot);
  const glm::mat4 Mbake = mWheelBake[wheelIndex];
  const glm::mat4 C =
      glm::rotate(glm::mat4(1.0f), -glm::pi<float>() * 0.5f, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::mat4(glm::mat3(Mbake));
  const glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(kModelScale));

  return Mphys * C * S * glm::inverse(Mbake);
}

void Vehicle::createTextureDescriptor()
{
  constexpr const char* kTexturePath = "assets/models/vehicle/textures/luaz_transparent_baseColor.png";

  auto texData = ptvc::io::loadTextureFromFile(kTexturePath, STBI_rgb_alpha);
  if(!texData.pixels)
  {
    exitWithError("Failed to load vehicle texture: {}", kTexturePath);
  }

  spdlog::info("Vehicle texture loaded from disk: {}x{} ({} channels)", texData.width, texData.height, texData.channels);

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
      .label      = "Vehicle-Texture",
      .device     = mVulkanContext->getDevice(),
  });
  exitOnError(imageResult);
  mTexture = std::move(imageResult.value());

  // Copy from staging buffer to image and generate mipmaps
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

  // Generate mipmaps and transition to shader-readable layout
  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    mTexture->generateMipmaps(
        cb,
        std::make_optional<ptvc::rhi::ImageState>({vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits2::eTransferWrite,
                                                   vk::PipelineStageFlagBits2::eTransfer}),
        std::make_optional<ptvc::rhi::ImageState>({vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead,
                                                   vk::PipelineStageFlagBits2::eFragmentShader}));
  });

  // Sampler
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
      .label    = "Vehicle-TextureDescriptor",
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

void Vehicle::createRenderResources(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
{
  using enum vk::ShaderStageFlagBits;

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, sceneDescriptor)
                      .addDescriptor(1, mTextureDescriptor)
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(ptvc::GPUGameObjectData)})
                      .addVertexType<ptvc::Vertex>()
                      .addShader({"assets/shaders/phong.vert.glsl", eVertex})
                      .addShader({"assets/shaders/phong.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .setName("VehiclePipeline")
                      .create(mVulkanContext->getDevice());

  constexpr const char* kModelPath = "assets/models/vehicle/scene.gltf";

  ptvc::glTF_Options bodyOpts;
  bodyOpts.excludeNodes = {"wheel_FR", "wheel_FL", "wheel_RL", "wheel_RR"};
  auto body             = makeUnique<ptvc::glTF>(kModelPath, bodyOpts);
  body->init(mVulkanContext.get());

  for(int i = 0; i < kWheelCount; ++i)
  {
    mWheelBake[i] = body->nodeWorldMatrix(kWheels[i].node);

    auto wheel = makeUnique<ptvc::glTF>(kModelPath, ptvc::glTF_Options{.includeNodes = {kWheels[i].node}});
    wheel->init(mVulkanContext.get());
    mWheelGeometry[i] = std::move(wheel);
  }

  mPipeline = std::move(pipeline);
  mGeometry = std::move(body);
}
