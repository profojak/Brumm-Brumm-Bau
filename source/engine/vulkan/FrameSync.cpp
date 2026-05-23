#include "FrameSync.hpp"

#include "Device.hpp"

namespace ptvc::rhi {
FrameSync::FrameSync(const SPtr<Device>& device, const uint32_t framesInFlight)
    : mDevice(device)
    , mFramesInFlight(framesInFlight)
    , mCurrentFrame(0u)
{
  constexpr auto semaphoreCreateInfo = vk::SemaphoreCreateInfo();
  constexpr auto fenceCreateInfo     = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);

  mPresentFinished.resize(framesInFlight);
  mImageAvailable.resize(framesInFlight);
  mRenderingFinished.resize(framesInFlight);

  const auto d = mDevice->getHandle();
  for(auto i = 0; i < mFramesInFlight; i++)
  {
    mPresentFinished[i]   = d.createFence(fenceCreateInfo);
    mImageAvailable[i]    = d.createSemaphore(semaphoreCreateInfo);
    mRenderingFinished[i] = d.createSemaphore(semaphoreCreateInfo);
  }
}

FrameSync::~FrameSync()
{
  const auto d = mDevice->getHandle();
  d.waitIdle();
  for(auto i = 0; i < mFramesInFlight; i++)
  {
    d.destroy(mPresentFinished[i]);
    d.destroy(mImageAvailable[i]);
    d.destroy(mRenderingFinished[i]);
  }
}

Frame FrameSync::getNextFrame() const noexcept
{
  return {
      .fPresentFinished   = mPresentFinished[mCurrentFrame],
      .sImageAvailable    = mImageAvailable[mCurrentFrame],
      .sRenderingFinished = mRenderingFinished[mCurrentFrame],
      .currentFrameIndex  = mCurrentFrame,
  };
}

void FrameSync::advance() noexcept
{
  mCurrentFrame = (mCurrentFrame + 1) % mFramesInFlight;
}
}  // namespace ptvc::rhi
