#include "Checkpoints.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <unordered_map>

#include <stb_image.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <render/GraphicsPipeline.hpp>
#include <scene/Vertex.hpp>

namespace {

struct alignas(16) CheckpointPC
{
  glm::mat4 model;
  glm::vec4 color;
};

struct alignas(16) StarPC
{
  glm::mat4 baseModel;
  glm::vec4 colorSpin;
};

class UnitCylinder : public ptvc::Geometry
{
public:
  explicit UnitCylinder(int segments)
      : Geometry("CheckpointCylinder")
  {
    for(int i = 0; i < segments; ++i)
    {
      const float     a = static_cast<float>(i) / static_cast<float>(segments) * glm::two_pi<float>();
      const float     x = std::cos(a);
      const float     z = std::sin(a);
      const float     u = static_cast<float>(i) / static_cast<float>(segments);
      const glm::vec3 n = glm::vec3(x, 0.0f, z);

      mVertices.push_back({glm::vec3(x, 0.0f, z), n, glm::vec2(u, 0.0f)});
      mVertices.push_back({glm::vec3(x, 1.0f, z), n, glm::vec2(u, 1.0f)});
    }

    for(int i = 0; i < segments; ++i)
    {
      const uint32_t b0 = static_cast<uint32_t>(i) * 2u;
      const uint32_t t0 = b0 + 1u;
      const uint32_t b1 = static_cast<uint32_t>((i + 1) % segments) * 2u;
      const uint32_t t1 = b1 + 1u;

      mIndices.insert(mIndices.end(), {b0, b1, t1});
      mIndices.insert(mIndices.end(), {b0, t1, t0});
    }
  }
};

float approachFactor(float distance, float range) noexcept
{
  if(distance <= 0.0f)
    return 1.0f;
  if(distance >= range)
    return 0.0f;
  const float t = distance / range;
  const float s = t * t * (3.0f - 2.0f * t);
  return 1.0f - s;
}
}  // namespace

Checkpoints::Checkpoints(const ptvc::GameObjectParams&      params,
                         SPtr<ptvc::rhi::VulkanContext>     vulkanContext,
                         const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
    : GameObject(params)
    , mVulkanContext(std::move(vulkanContext))
{
  assert(mVulkanContext && "Checkpoints requires a valid VulkanContext pointer");

  loadFromProps();
  loadHeightmapAndPlaceOnTerrain();

  mSpinAngles.assign(mCheckpoints.size(), 0.0f);

  createCylinderGeometry();
  createCylinderPipeline(sceneDescriptor);

  loadStar();
  createStarPipeline(sceneDescriptor);
}

Checkpoints::~Checkpoints() = default;

void Checkpoints::collectDebugMeshes(std::vector<ptvc::GameObject::DebugMesh>& out) const noexcept
{
  if(mCheckpoints.empty() || mCurrentCheckpoint >= mCheckpoints.size())
    return;

  const auto& cp = mCheckpoints[mCurrentCheckpoint];

  if(mCylinder)
  {
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), cp.position)
        * glm::scale(glm::mat4(1.0f), glm::vec3(kCylinderRadius, kCylinderHeight, kCylinderRadius));
    out.push_back({model, mCylinder.get()});
  }

  if(mStar)
  {
    const glm::vec3 starPos  = cp.position + glm::vec3(0.0f, kStarHoverHeight, 0.0f);
    const glm::mat4 model    = glm::translate(glm::mat4(1.0f), starPos) * glm::scale(glm::mat4(1.0f), glm::vec3(kStarScale));
    out.push_back({model, mStar.get()});
  }
}

void Checkpoints::onUpdate(const float dt, const ptvc::rhi::Frame& /*frame*/) noexcept
{
  if(mCheckpoints.empty())
    return;

  glm::vec3 vehiclePos{};
  if(mVehiclePositionProvider)
    vehiclePos = mVehiclePositionProvider();

  if(mCurrentCheckpoint < mCheckpoints.size())
  {
    const auto& cp       = mCheckpoints[mCurrentCheckpoint];
    const float distance = glm::distance(vehiclePos, cp.position);

    if(distance <= kReachDistance)
    {
      ++mCurrentCheckpoint;
      if(mCurrentCheckpoint >= mCheckpoints.size())
        spdlog::info("All checkpoints collected!");
    }
  }

  if(mCurrentCheckpoint >= mCheckpoints.size())
  {
    return;
  }

  const float distance = glm::distance(vehiclePos, mCheckpoints[mCurrentCheckpoint].position);
  const float factor   = approachFactor(distance, kApproachRange);
  const float speed    = kSpinBaseSpeed + factor * kSpinBoostSpeed;

  mSpinAngles[mCurrentCheckpoint] += speed * dt;
}

void Checkpoints::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  if(mCheckpoints.empty() || mCurrentCheckpoint >= mCheckpoints.size())
    return;

  const auto& cp = mCheckpoints[mCurrentCheckpoint];

  // Transparent checkpoint cylinder.
  if(mCylinderPipeline && mCylinder)
  {
    mCylinderPipeline->bind(frame, frame.commandBuffer);

    const glm::mat4 model = glm::translate(glm::mat4(1.0f), cp.position)
                            * glm::scale(glm::mat4(1.0f), glm::vec3(kCylinderRadius, kCylinderHeight, kCylinderRadius));

    const CheckpointPC pc{.model = model, .color = kCylinderColor};
    mCylinderPipeline->pushConstant(&pc, frame.commandBuffer);
    mCylinder->draw(frame.commandBuffer);
  }

  // Animated star.
  if(mStarPipeline && mStar)
  {
    mStarPipeline->bind(frame, frame.commandBuffer);

    const glm::vec3 starPos = cp.position + glm::vec3(0.0f, kStarHoverHeight, 0.0f);
    const glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), starPos) * glm::scale(glm::mat4(1.0f), glm::vec3(kStarScale));

    const StarPC pc{
        .baseModel = baseModel,
        .colorSpin = glm::vec4(kStarColor, mSpinAngles[mCurrentCheckpoint]),
    };
    mStarPipeline->pushConstant(&pc, frame.commandBuffer);
    mStar->draw(frame.commandBuffer);
  }
}

void Checkpoints::loadFromProps()
{
  constexpr const char* kPropsPath = "assets/textures/props.png";

  int            width = 0, height = 0, channels = 0;
  const stbi_uc* pixels = stbi_load(kPropsPath, &width, &height, &channels, STBI_rgb);

  if(!pixels)
  {
    exitWithError("Failed to load props texture: {}", kPropsPath);
    return;
  }

  // Collect every red pixel (R = 255, G = 0). The blue channel encodes the
  // checkpoint order, where 0 is the first checkpoint.
  std::unordered_map<uint32_t, std::vector<glm::vec2>> byIndex;

  for(int y = 0; y < height; ++y)
  {
    for(int x = 0; x < width; ++x)
    {
      const stbi_uc* p = pixels + (static_cast<ptrdiff_t>(y) * width + x) * 3;
      const stbi_uc  r = p[0];
      const stbi_uc  g = p[1];
      const stbi_uc  b = p[2];

      if(r == 255 && g == 0)
        byIndex[b].emplace_back(static_cast<float>(x), static_cast<float>(y));
    }
  }

  stbi_image_free((void*)pixels);

  if(byIndex.empty())
  {
    spdlog::warn("props.png contained no checkpoint pixels (R=255, G=0)");
    return;
  }

  mCheckpoints.clear();
  mCheckpoints.reserve(byIndex.size());

  for(const auto& [index, pixelsAt] : byIndex)
  {
    glm::vec2 centroid(0.0f);
    for(const auto& px : pixelsAt)
      centroid += px;
    centroid /= static_cast<float>(pixelsAt.size());

    const float u = centroid.x / static_cast<float>(width - 1);
    const float v = centroid.y / static_cast<float>(height - 1);

    Checkpoint cp;
    cp.index    = index;
    cp.position = glm::vec3(u * kTerrainWorldSize - kTerrainHalfSize, 0.0f, v * kTerrainWorldSize - kTerrainHalfSize);
    mCheckpoints.push_back(cp);
  }

  std::ranges::sort(mCheckpoints, {}, &Checkpoint::index);
}

void Checkpoints::loadHeightmapAndPlaceOnTerrain()
{
  if(mCheckpoints.empty())
    return;

  constexpr const char* kHeightmapPath = "assets/textures/heightmap.png";

  int            width = 0, height = 0, channels = 0;
  const stbi_us* pixels = stbi_load_16(kHeightmapPath, &width, &height, &channels, 1);

  if(!pixels)
  {
    exitWithError("Failed to load heightmap for checkpoint sampling: {}", kHeightmapPath);
    return;
  }

  // For every checkpoint, snap its world XZ back to the nearest heightmap
  // texel and read the terrain height so the cylinder sits on the surface.
  for(auto& cp : mCheckpoints)
  {
    const float u = (cp.position.x + kTerrainHalfSize) / kTerrainWorldSize;
    const float v = (cp.position.z + kTerrainHalfSize) / kTerrainWorldSize;

    int hx = static_cast<int>(std::round(u * static_cast<float>(width - 1)));
    int hy = static_cast<int>(std::round(v * static_cast<float>(height - 1)));
    hx     = std::clamp(hx, 0, width - 1);
    hy     = std::clamp(hy, 0, height - 1);

    const uint16_t raw        = pixels[static_cast<ptrdiff_t>(hy) * width + hx];
    const float    normalized = static_cast<float>(raw) / 65535.0f;

    cp.position.y = normalized * kHeightScale;
  }

  stbi_image_free((void*)pixels);
}

void Checkpoints::createCylinderGeometry()
{
  constexpr int kSegments = 48;

  mCylinder = makeUnique<UnitCylinder>(kSegments);
  mCylinder->init(mVulkanContext.get());
}

void Checkpoints::createCylinderPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
{
  using enum vk::ShaderStageFlagBits;
  using enum vk::ColorComponentFlagBits;

  // Standard alpha blending for a transparent cylinder.
  const auto blendAttachment = ptvc::rhi::detail::makeColorBlendAttachmentState(
      eR | eG | eB | eA, VK_TRUE, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
      vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, sceneDescriptor)
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(CheckpointPC)})
                      .addVertexType<ptvc::Vertex>()
                      .addShader({"assets/shaders/checkpoint.vert.glsl", eVertex})
                      .addShader({"assets/shaders/checkpoint.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm, blendAttachment)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                        state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                        state.depthStencilState.setDepthWriteEnable(false);
                      })
                      .setName("CheckpointPipeline")
                      .create(mVulkanContext->getDevice());

  mCylinderPipeline = std::move(pipeline);
}

void Checkpoints::loadStar()
{
  constexpr const char* kStarPath = "assets/models/star/scene.gltf";

  auto star = makeUnique<ptvc::glTF>(kStarPath);
  star->init(mVulkanContext.get());

  if(star->getVertices().empty())
  {
    spdlog::error("Star glTF loaded no geometry from '{}'", kStarPath);
    return;
  }

  mStar = std::move(star);
}

void Checkpoints::createStarPipeline(const SPtr<ptvc::rhi::Descriptor>& sceneDescriptor)
{
  using enum vk::ShaderStageFlagBits;

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, sceneDescriptor)
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(StarPC)})
                      .addVertexType<ptvc::Vertex>()
                      .addShader({"assets/shaders/star.vert.glsl", eVertex})
                      .addShader({"assets/shaders/star.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                        state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                      })
                      .setName("StarPipeline")
                      .create(mVulkanContext->getDevice());

  mStarPipeline = std::move(pipeline);
}
