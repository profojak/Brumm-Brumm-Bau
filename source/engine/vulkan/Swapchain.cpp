#include "Swapchain.hpp"

#include "Device.hpp"
#include "IWindow.hpp"

namespace ptvc::rhi {
namespace detail {
// Preferred Swapchain format options
constexpr auto gPreferredFormat     = vk::Format::eB8G8R8A8Unorm;
constexpr auto gPreferredColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

// Preferred Swapchain present mode
constexpr auto gPreferredPresentMode = vk::PresentModeKHR::eMailbox;

// Evaluate supported formats for a given Surface and try to select the preferred format options.
[[nodiscard]] static Result<vk::SurfaceFormatKHR> getSurfaceFormat(const vk::PhysicalDevice physicalDevice,
                                                                   const vk::SurfaceKHR     surface) noexcept
{
  const auto supportedFormats = physicalDevice.getSurfaceFormatsKHR(surface);
  if(supportedFormats.empty())
  {
    return std::unexpected("The specified Surface supports no surface formats");
  }

  for(const auto& fmt : supportedFormats)
  {
    if(fmt.format == gPreferredFormat && fmt.colorSpace == gPreferredColorSpace)
    {
      return fmt;
    }
  }

  return supportedFormats[0];
}

// Evaluate supported present modes for a given Surface and try to select the preferred one.
[[nodiscard]] static Result<vk::PresentModeKHR> getPresentMode(const vk::PhysicalDevice physicalDevice, const vk::SurfaceKHR surface) noexcept
{
  const auto supportedModes = physicalDevice.getSurfacePresentModesKHR(surface);
  if(supportedModes.empty())
  {
    return std::unexpected("The specified Surface supports no present modes");
  }

  return contains(supportedModes, gPreferredPresentMode) ? gPreferredPresentMode : supportedModes[0];
}

// Ensure the extent is within the supported bounds.
[[nodiscard]] static vk::Extent2D getSwapchainExtent(const IWindow* pWindow, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) noexcept
{
  vk::Extent2D extent;
  if(surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    extent = surfaceCapabilities.currentExtent;
  }

  const vk::Extent2D min = surfaceCapabilities.minImageExtent;
  const vk::Extent2D max = surfaceCapabilities.maxImageExtent;

  auto [width, height] = pWindow->getFramebufferExtent();
  extent.width         = std::clamp(width, min.width, max.width);
  extent.height        = std::clamp(height, min.height, max.height);

  return extent;
}
}  // namespace detail

Result<UPtr<Swapchain>> Swapchain::create(const SwapchainCreateInfo& createInfo) noexcept
{
  auto swapchain = UPtr<Swapchain>(std::move(new Swapchain(createInfo)));
  if(const auto result = swapchain->createSwapchain(createInfo.surface, createInfo.pOldSwapchain); !result.has_value())
  {
    return std::unexpected(result.error());
  }

  spdlog::debug("Created new Swapchain [w={}, h={}, imageCount={}, format={}]", swapchain->mExtent.width,
                swapchain->mExtent.height, swapchain->mImageCount, vk::to_string(swapchain->mFormat));

  return std::move(swapchain);
}

Swapchain::~Swapchain()
{
  const auto d = mDevice->getHandle();
  for(const auto& view : mImageViews)
  {
    d.destroy(view);
  }
  d.destroy(mSwapchain);
}

vk::ImageMemoryBarrier2 Swapchain::getBarrier(const uint32_t i, const ImageState& dstState) noexcept
{
  assert(i < mImages.size());
  auto& imageState = mImageStates[i];

  const auto barrier = vk::ImageMemoryBarrier2()
                           .setImage(mImages[i])
                           .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                           .setOldLayout(imageState.layout)
                           .setSrcAccessMask(imageState.accessMask)
                           .setSrcStageMask(imageState.stageMask)
                           .setNewLayout(dstState.layout)
                           .setDstAccessMask(dstState.accessMask)
                           .setDstStageMask(dstState.stageMask);

  // Update tracked state (this assumes all barriers from this function are executed...)
  imageState = dstState;

  return barrier;
}

vk::Image Swapchain::getImage(const uint32_t i) const noexcept
{
  assert(i < mImages.size());
  return mImages[i];
}

vk::ImageView Swapchain::getImageView(const uint32_t i) const noexcept
{
  assert(i < mImageViews.size());
  return mImageViews[i];
}

vk::SwapchainKHR Swapchain::getHandle() const noexcept
{
  return mSwapchain;
}

uint32_t Swapchain::getImageCount() const noexcept
{
  return mImageCount;
}

vk::Format Swapchain::getFormat() const noexcept
{
  return mFormat;
}

vk::Extent2D Swapchain::getExtent() const noexcept
{
  return mExtent;
}

Swapchain::Swapchain(const SwapchainCreateInfo& createInfo)
    : mWindow(createInfo.window)
    , mDevice(createInfo.device)
{
}

Result<void> Swapchain::createSwapchain(const vk::SurfaceKHR surface, const Swapchain* pOldSwapchain) noexcept
{
  const auto surfaceFormat = detail::getSurfaceFormat(mDevice->getPhysicalDevice(), surface);
  if(!surfaceFormat.has_value())
  {
    return std::unexpected(surfaceFormat.error());
  }
  mFormat = surfaceFormat.value().format;

  const auto presentMode = detail::getPresentMode(mDevice->getPhysicalDevice(), surface);
  if(!presentMode.has_value())
  {
    return std::unexpected(presentMode.error());
  }

  const auto surfaceCapabilities = mDevice->getPhysicalDevice().getSurfaceCapabilitiesKHR(surface);
  mExtent                        = detail::getSwapchainExtent(mWindow.get(), surfaceCapabilities);

  const auto createInfo = vk::SwapchainCreateInfoKHR()
                              .setSurface(surface)
                              .setMinImageCount(mImageCount)
                              .setImageFormat(surfaceFormat.value().format)
                              .setImageColorSpace(surfaceFormat.value().colorSpace)
                              .setPresentMode(presentMode.value())
                              .setImageExtent(mExtent)
                              .setPreTransform(surfaceCapabilities.currentTransform)
                              .setImageArrayLayers(1)
                              .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
                              .setClipped(true)
                              .setOldSwapchain(pOldSwapchain ? pOldSwapchain->getHandle() : nullptr)
                              .setImageSharingMode(vk::SharingMode::eExclusive)
                              .setQueueFamilyIndexCount(0)
                              .setPQueueFamilyIndices(nullptr);

  VK_CATCH(mSwapchain = mDevice->getHandle().createSwapchainKHR(createInfo));

  // Get Images from the Swapchain
  mImages.resize(mImageCount);
  vk::Result result = mDevice->getHandle().getSwapchainImagesKHR(mSwapchain, &mImageCount, mImages.data());
  VK_RESULT(result);

  // Create ImageViews for each Image
  constexpr vk::ComponentMapping componentMapping = {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                                                     vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};

  auto imageViewCreateInfo = vk::ImageViewCreateInfo()
                                 .setComponents(componentMapping)
                                 .setFormat(surfaceFormat.value().format)
                                 .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                                 .setViewType(vk::ImageViewType::e2D);

  mImageViews.resize(mImageCount);
  for(auto i = 0; i < mImageViews.size(); i++)
  {
    mImageStates.push_back({});

    imageViewCreateInfo.setImage(mImages[i]);
    result = mDevice->getHandle().createImageView(&imageViewCreateInfo, nullptr, &mImageViews[i]);
    VK_RESULT(result);
  }

  for(auto i = 0; i < mImageCount; i++)
  {
    mDevice->setLabel<vk::Image>({
        .name   = std::format("SwapchainImage[{}]", i),
        .handle = mImages[i],
    });
    mDevice->setLabel<vk::ImageView>({
        .name   = std::format("SwapchainImageView[{}]", i),
        .handle = mImageViews[i],
    });
  }

  return {};
}
}  // namespace ptvc::rhi
