#pragma once

#include <string_view>
#include <lib/size.hpp>

namespace ptvc::wsi
{
    struct WindowCreateInfo
    {
        Size2D           size         = { 1280u, 720u };
        std::string_view title        = "ptvc-framework";
    };
}
