#pragma once

#include "ext/Extensions.hpp"

namespace ptvc::rhi {
struct VulkanContextOptions
{
  // Function used to configure Ex
  std::function<void(Extensions&)> extensions;
};
}  // namespace ptvc::rhi
