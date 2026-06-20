#include "Timer.hpp"
#include "Checkpoints.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include <stb_image.h>
#include <SDL3/SDL_events.h>
#include <spdlog/spdlog.h>

#include <render/GraphicsPipeline.hpp>
#include <scene/Vertex.hpp>

namespace {

struct alignas(16) TextPC
{
  uint32_t chars[16];  // 64 bytes
  float    x;
  float    y;
  float    charW;
  float    charH;
  float    gap;
  float    screenW;
  float    screenH;
  uint32_t count;
};

// One quad (4 vertices, 6 indices) per character slot. The vertex data only
// encodes the corner offset; the actual glyph selection and screen placement
// are derived from gl_VertexIndex in the vertex shader.
class TextQuads : public ptvc::Geometry
{
public:
  explicit TextQuads(uint32_t slotCount)
      : Geometry("TextQuads")
  {
    mVertices.reserve(slotCount * 4);
    mIndices.reserve(slotCount * 6);

    for(uint32_t s = 0; s < slotCount; ++s)
    {
      const uint32_t base = s * 4;
      mVertices.push_back({glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 0.0f)});
      mVertices.push_back({glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 0.0f)});
      mVertices.push_back({glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 1.0f)});
      mVertices.push_back({glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 1.0f)});

      mIndices.insert(mIndices.end(), {base + 0, base + 1, base + 2});
      mIndices.insert(mIndices.end(), {base + 2, base + 1, base + 3});
    }
  }
};

// Map an ASCII character to its index in the glyph atlas.
uint32_t glyphIndex(char c) noexcept
{
  if(c >= '0' && c <= '9')
    return static_cast<uint32_t>(c - '0');
  if(c == ':')
    return 10;
  if(c == '/')
    return 11;
  return 11;
}

// Two-digit zero-padded formatting into a fixed buffer.
std::string twoDigits(uint32_t value) noexcept
{
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02u", value % 100u);
  return buf;
}

}  // namespace

Timer::Timer(const ptvc::GameObjectParams& params, SPtr<ptvc::rhi::VulkanContext> vulkanContext, Checkpoints* checkpoints)
    : GameObject(params)
    , mVulkanContext(std::move(vulkanContext))
    , mCheckpoints(checkpoints)
{
  assert(mVulkanContext && "Timer requires a valid VulkanContext pointer");

  loadTextTexture();
  createTextDescriptor();
  createTextGeometry();
  createTextPipeline();
  createWireframePipeline();
}

Timer::~Timer()
{
  if(mSampler)
    mVulkanContext->getDevice()->getHandle().destroySampler(mSampler);
}

void Timer::onEvent(const SDL_Event& event) noexcept
{
  // Start the timer on the very first key press of any kind.
  if(!mStarted && event.type == SDL_EVENT_KEY_DOWN)
  {
    mStarted   = true;
    mStartTime = std::chrono::steady_clock::now();
  }
}

void Timer::onUpdate(const float /*dt*/, const ptvc::rhi::Frame& /*frame*/) noexcept
{
  if(!mStarted)
    return;

  if(!mStopped && mCheckpoints && mCheckpoints->isFinished())
  {
    mStopped = true;
    mElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - mStartTime).count();
  }

  if(!mStopped)
    mElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - mStartTime).count();
}

void Timer::onRender(const ptvc::rhi::Frame& frame) noexcept
{
  if(!mPipeline || !mGeometry || !mDescriptor || !mTextTexture)
    return;

  const auto  extent  = mVulkanContext->getSwapchain()->getExtent();
  const float screenW = static_cast<float>(extent.width);
  const float screenH = static_cast<float>(extent.height);

  const float charW = static_cast<float>(kGlyphWidth) * kGlyphScale;
  const float gap   = kGlyphGap;

  mPipeline->bind(frame, frame.commandBuffer);

  const double   total      = mElapsed;
  const uint32_t minutes    = static_cast<uint32_t>(total) / 60u;
  const uint32_t seconds    = static_cast<uint32_t>(total) % 60u;
  const uint32_t hundredths = static_cast<uint32_t>(total * 100.0) % 100u;

  std::string timerStr = twoDigits(minutes) + ":" + twoDigits(seconds) + ":" + twoDigits(hundredths);
  drawText(frame, timerStr.c_str(), kMargin, kMargin, screenW, screenH, mPipeline);

  uint32_t collected = 0;
  uint32_t totalCps  = 0;
  if(mCheckpoints)
  {
    collected = static_cast<uint32_t>(mCheckpoints->getCollectedCount());
    totalCps  = static_cast<uint32_t>(mCheckpoints->getCheckpoints().size());
  }

  std::string counterStr = twoDigits(collected) + "/" + twoDigits(totalCps);
  const float counterX =
      screenW - kMargin - static_cast<float>(counterStr.size()) * charW - static_cast<float>(counterStr.size() - 1) * gap;
  drawText(frame, counterStr.c_str(), counterX, kMargin, screenW, screenH, mPipeline);
}

void Timer::onRenderDebug(const ptvc::rhi::Frame& frame) noexcept
{
  if(!mWireframePipeline || !mGeometry)
    return;

  const auto  extent  = mVulkanContext->getSwapchain()->getExtent();
  const float screenW = static_cast<float>(extent.width);
  const float screenH = static_cast<float>(extent.height);

  const float charW = static_cast<float>(kGlyphWidth) * kGlyphScale;
  const float gap   = kGlyphGap;

  mWireframePipeline->bind(frame, frame.commandBuffer);

  const double   total      = mElapsed;
  const uint32_t minutes    = static_cast<uint32_t>(total) / 60u;
  const uint32_t seconds    = static_cast<uint32_t>(total) % 60u;
  const uint32_t hundredths = static_cast<uint32_t>(total * 100.0) % 100u;

  std::string timerStr = twoDigits(minutes) + ":" + twoDigits(seconds) + ":" + twoDigits(hundredths);
  drawText(frame, timerStr.c_str(), kMargin, kMargin, screenW, screenH, mWireframePipeline);

  uint32_t collected = 0;
  uint32_t totalCps  = 0;
  if(mCheckpoints)
  {
    collected = static_cast<uint32_t>(mCheckpoints->getCollectedCount());
    totalCps  = static_cast<uint32_t>(mCheckpoints->getCheckpoints().size());
  }

  std::string counterStr = twoDigits(collected) + "/" + twoDigits(totalCps);
  const float counterX =
      screenW - kMargin - static_cast<float>(counterStr.size()) * charW - static_cast<float>(counterStr.size() - 1) * gap;
  drawText(frame, counterStr.c_str(), counterX, kMargin, screenW, screenH, mWireframePipeline);
}

void Timer::drawText(const ptvc::rhi::Frame&          frame,
                     const char*                      str,
                     float                            x,
                     float                            y,
                     float                            screenW,
                     float                            screenH,
                     const SPtr<ptvc::rhi::Pipeline>& pipeline) noexcept
{
  TextPC pc{};
  pc.charW   = static_cast<float>(kGlyphWidth) * kGlyphScale;
  pc.charH   = static_cast<float>(kGlyphHeight) * kGlyphScale;
  pc.gap     = kGlyphGap;
  pc.screenW = screenW;
  pc.screenH = screenH;
  pc.x       = x;
  pc.y       = y;

  uint32_t count = 0;
  for(; str[count] != '\0' && count < kMaxSlots; ++count)
    pc.chars[count] = glyphIndex(str[count]);
  pc.count = count;

  pipeline->pushConstant(&pc, frame.commandBuffer);
  mGeometry->draw(frame.commandBuffer);
}

void Timer::loadTextTexture()
{
  constexpr const char* kPath = "assets/textures/text.png";

  int            width = 0, height = 0, channels = 0;
  const stbi_uc* pixels = stbi_load(kPath, &width, &height, &channels, STBI_rgb_alpha);

  if(!pixels)
  {
    exitWithError("Failed to load text atlas: {}", kPath);
    return;
  }

  if(width != static_cast<int>(kAtlasGlyphCount * kGlyphWidth) || height != static_cast<int>(kGlyphHeight))
  {
    spdlog::warn("Text atlas '{}' has unexpected size {}x{} (expected {}x{})", kPath, width, height,
                 kAtlasGlyphCount * kGlyphWidth, kGlyphHeight);
  }

  const auto imageSize = static_cast<vk::DeviceSize>(width) * static_cast<vk::DeviceSize>(height) * 4;

  const auto stagingResult = ptvc::rhi::Buffer::create({
      .size        = imageSize,
      .hostVisible = true,
      .label       = "Text-Staging",
      .device      = mVulkanContext->getDevice(),
  });
  exitOnError(stagingResult);
  auto staging = std::move(stagingResult.value());

  staging->setData(pixels, imageSize, 0);
  stbi_image_free((void*)pixels);

  const auto imageResult = ptvc::rhi::Image::create({
      .extent     = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
      .usageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      .format     = vk::Format::eR8G8B8A8Srgb,
      .mipmapping = false,
      .label      = "Text-Atlas",
      .device     = mVulkanContext->getDevice(),
  });
  exitOnError(imageResult);
  mTextTexture = std::move(imageResult.value());

  mVulkanContext->executeImmediateCommand([&](const vk::CommandBuffer& cb) {
    const auto toDst = vk::ImageMemoryBarrier2()
                           .setImage(mTextTexture->getHandle())
                           .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                           .setOldLayout(vk::ImageLayout::eUndefined)
                           .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                           .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                           .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                           .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
                           .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);

    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(toDst));

    const auto region = vk::BufferImageCopy2()
                            .setBufferOffset(0)
                            .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                            .setImageExtent({static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1});

    cb.copyBufferToImage2(vk::CopyBufferToImageInfo2()
                              .setSrcBuffer(staging->getHandle())
                              .setDstImage(mTextTexture->getHandle())
                              .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
                              .setRegions(region));

    const auto toRead = vk::ImageMemoryBarrier2()
                            .setImage(mTextTexture->getHandle())
                            .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
                            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
                            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                            .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                            .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader);

    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(toRead));
  });

  // Nearest filtering for pixel-art look!
  auto samplerInfo = vk::SamplerCreateInfo()
                         .setMagFilter(vk::Filter::eNearest)
                         .setMinFilter(vk::Filter::eNearest)
                         .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                         .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                         .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                         .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                         .setMinLod(0.0f)
                         .setMaxLod(0.0f)
                         .setBorderColor(vk::BorderColor::eFloatTransparentBlack);

  mSampler = mVulkanContext->getDevice()->getHandle().createSampler(samplerInfo);
}

void Timer::createTextGeometry()
{
  mGeometry = makeUnique<TextQuads>(kMaxSlots);
  mGeometry->init(mVulkanContext.get());
}

void Timer::createTextDescriptor()
{
  const auto descResult = ptvc::rhi::Descriptor::create({
      .bindings =
          {
              {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
          },
      .setCount = mVulkanContext->getSwapchain()->getImageCount(),
      .label    = "TextDescriptor",
      .device   = mVulkanContext->getDevice(),
  });
  exitOnError(descResult);
  mDescriptor = std::move(descResult.value());

  for(uint32_t i = 0; i < mDescriptor->getSetCount(); ++i)
  {
    const auto imageInfo =
        vk::DescriptorImageInfo().setSampler(mSampler).setImageView(mTextTexture->getImageView()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    const auto write = vk::WriteDescriptorSet()
                           .setImageInfo(imageInfo)
                           .setDstBinding(0)
                           .setDescriptorCount(1)
                           .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                           .setDstSet(mDescriptor->getSet(i));

    mVulkanContext->getDevice()->getHandle().updateDescriptorSets(write, {});
  }
}

void Timer::createTextPipeline()
{
  using enum vk::ShaderStageFlagBits;
  using enum vk::ColorComponentFlagBits;

  const auto blendAttachment = ptvc::rhi::detail::makeColorBlendAttachmentState(
      eR | eG | eB | eA, VK_TRUE, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
      vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addDescriptor(0, mDescriptor)
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(TextPC)})
                      .addVertexType<ptvc::Vertex>()
                      .addShader({"assets/shaders/text.vert.glsl", eVertex})
                      .addShader({"assets/shaders/text.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm, blendAttachment)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                        state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                        state.depthStencilState.setDepthTestEnable(false);
                        state.depthStencilState.setDepthWriteEnable(false);
                      })
                      .setName("TextPipeline")
                      .create(mVulkanContext->getDevice());

  mPipeline = std::move(pipeline);
}

void Timer::createWireframePipeline()
{
  using enum vk::ShaderStageFlagBits;

  auto pipeline = ptvc::rhi::GraphicsPipelineBuilder()
                      .addPushConstantRange({eVertex | eFragment, 0, sizeof(TextPC)})
                      .addVertexType<ptvc::Vertex>()
                      .addShader({"assets/shaders/text.vert.glsl", eVertex})
                      .addShader({"assets/shaders/text_wire.frag.glsl", eFragment})
                      .addAttachment(vk::Format::eB8G8R8A8Unorm)
                      .setDepthFormat(vk::Format::eD32Sfloat)
                      .configure([](ptvc::rhi::GraphicsPipelineState& state) {
                        state.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
                        state.rasterizationState.setPolygonMode(vk::PolygonMode::eLine);
                        state.depthStencilState.setDepthTestEnable(false);
                        state.depthStencilState.setDepthWriteEnable(false);
                      })
                      .setName("TextWireframePipeline")
                      .create(mVulkanContext->getDevice());

  mWireframePipeline = std::move(pipeline);
}
