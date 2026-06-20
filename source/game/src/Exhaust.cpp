#include "Exhaust.hpp"
#include "Vehicle.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include <scene/primitives/Cube.hpp>
#include <scene/Vertex.hpp>
#include <render/GraphicsPipeline.hpp>
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

Exhaust::Exhaust(const ptvc::GameObjectParams&      params,
                 Vehicle*                           vehicle,
                 SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
                 const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
    : GameObject(params)
    , mVulkanContext(std::move(vulkanContext))
    , mVehicle(vehicle)
{
  assert(mVulkanContext && "Exhaust requires a valid VulkanContext pointer");

  createGeometry();
  createInstanceBuffer();
  createPipeline(sceneDescriptor);
}

void Exhaust::onUpdate(float dt, const ptvc::rhi::Frame& /*frame*/) noexcept
{
  for(auto& p : mParticles)
  {
    if(p.life <= 0.0f)
      continue;

    p.life -= dt;
    p.position.y += kRiseSpeed * dt;
    p.position += p.drift * dt;

    if(p.life <= 0.0f)
    {
      p.life  = 0.0f;
      p.scale = 0.0f;
    }
    else
    {
      const float t = p.life / p.maxLife;
      p.scale       = p.startScale * t;
    }
  }

  // Only spawn if accelerating
  if(mVehicle && mVehicle->isAccelerating())
  {
    mSpawnTimer += dt;
    while(mSpawnTimer >= kSpawnInterval)
    {
      mSpawnTimer -= kSpawnInterval;

      auto it = std::find_if(mParticles.begin(), mParticles.end(), [](const Particle& p) { return p.life <= 0.0f; });
      if(it == mParticles.end())
        break;

      const glm::vec3 carPos = mVehicle->getPosition();
      const glm::quat carRot = mVehicle->getRotation();

      const glm::vec3 back     = carRot * glm::vec3(-0.25f, -0.09f, -1.0f);
      const glm::vec3 spawnPos = carPos + back * 2.4f;

      static std::mt19937                          rng(0xBADF00Du);
      static std::uniform_real_distribution<float> j(-0.12f, 0.12f);
      static std::uniform_real_distribution<float> d(-0.15f, 0.15f);

      Particle p{};
      p.position   = spawnPos + glm::vec3(j(rng), 0.0f, j(rng));
      p.startScale = kStartScale;
      p.scale      = kStartScale;
      p.life       = kLifetime;
      p.maxLife    = kLifetime;
      p.drift      = glm::vec3(d(rng), 0.0f, d(rng)) + back * 0.25f;
      *it          = p;
    }
  }
  else
  {
    mSpawnTimer = 0.0f;
  }
}

void Exhaust::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  if(!mPipeline || !mCube || !mInstanceBuffer)
    return;

  std::array<glm::mat4, kMaxParticles> instances{};
  uint32_t                             aliveCount = 0;
  for(const auto& p : mParticles)
  {
    if(p.life <= 0.0f || p.scale <= 0.0f)
      continue;
    instances[aliveCount++] = glm::scale(glm::translate(glm::mat4(1.0f), p.position), glm::vec3(p.scale));
  }

  if(aliveCount == 0)
    return;

  mInstanceBuffer->setData(instances.data(), sizeof(glm::mat4) * aliveCount, 0);

  mPipeline->bind(frame, frame.commandBuffer);

  const ptvc::GPUGameObjectData pc = {
      .model              = glm::mat4(1.0f),
      .solidColor         = glm::vec4(0.55f, 0.55f, 0.55f, 1.0f),
      .materialProperties = {0.4f, 0.6f, 0.1f, 8.0f},
      .showFresnel        = 0,
      .useExampleTexture  = 0,
  };
  mPipeline->pushConstant(&pc, frame.commandBuffer);

  constexpr vk::DeviceSize offsets[2] = {0, 0};
  const vk::Buffer         buffers[2] = {
      mCube->getVertexBuffer()->getHandle(),
      mInstanceBuffer->getHandle(),
  };
  frame.commandBuffer.bindVertexBuffers(0, 2, buffers, offsets);
  frame.commandBuffer.bindIndexBuffer(mCube->getIndexBuffer()->getHandle(), 0, vk::IndexType::eUint32);
  frame.commandBuffer.drawIndexed(static_cast<uint32_t>(mCube->getIndices().size()), aliveCount, 0, 0, 0);
}

void Exhaust::createGeometry()
{
  mCube = makeUnique<ptvc::Cube>(1.0f);
  mCube->init(mVulkanContext.get());
}

void Exhaust::createInstanceBuffer()
{
  const auto size = sizeof(glm::mat4) * kMaxParticles;

  auto result = ptvc::rhi::Buffer::create({
      .size        = size,
      .usageFlags  = vk::BufferUsageFlagBits::eVertexBuffer,
      .hostVisible = true,
      .label       = "Exhaust-InstanceBuffer",
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(result);
  mInstanceBuffer = std::move(result.value());
}

void Exhaust::createPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
{
  using enum vk::ShaderStageFlagBits;

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, sceneDescriptor)
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(ptvc::GPUGameObjectData)})
                      .addVertexType<ptvc::Vertex>(0)
                      .addVertexType<InstanceMatrix>(1)
                      .addShader({"assets/shaders/exhaust.vert.glsl", eVertex})
                      .addShader({"assets/shaders/exhaust.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                        state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                      })
                      .setName("ExhaustPipeline")
                      .create(mVulkanContext->getDevice());

  mPipeline = std::move(pipeline);
}
