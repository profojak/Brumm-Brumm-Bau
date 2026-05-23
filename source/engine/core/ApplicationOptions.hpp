#pragma once
#include "VulkanContextOptions.hpp"
#include "wsi/WindowCreateInfo.hpp"

namespace ptvc
{
    struct ApplicationOptions
    {
        wsi::WindowCreateInfo     windowOptions = {};
        rhi::VulkanContextOptions vulkanOptions = {};
    };
}
