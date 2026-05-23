#pragma once

#include <vector>
#include "VulkanCore.hpp"

namespace ptvc::rhi {
class Device;

// Contains the synchronization objects for rendering.
class FrameSync
{
public:
  explicit FrameSync(const SPtr<Device>& device, uint32_t framesInFlight);

  ~FrameSync();

  // Return a new Frame object with synchronization objects for the current frame.
  [[nodiscard]] Frame getNextFrame() const noexcept;

  // Advance the current frame counter at the end of a frame.
  void advance() noexcept;

private:
  friend class VulkanContext;

  SPtr<Device> mDevice;

  uint32_t mFramesInFlight;
  uint32_t mCurrentFrame;

  std::vector<vk::Fence>     mPresentFinished;
  std::vector<vk::Semaphore> mImageAvailable;
  std::vector<vk::Semaphore> mRenderingFinished;
};
}  // namespace ptvc::rhi
