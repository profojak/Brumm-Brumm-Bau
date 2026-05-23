#pragma once

#include <format>
#include <string>

struct Size2D
{
    uint32_t width  = 0u;
    uint32_t height = 0u;
};

[[nodiscard]] inline std::string toString(const Size2D& size2D) noexcept
{
    return std::format("[w={}, h={}]", size2D.width, size2D.height);
}

struct Size3D
{
    uint32_t width  = 0u;
    uint32_t height = 0u;
    uint32_t depth  = 0u;
};

[[nodiscard]] inline std::string toString(const Size3D& size3D) noexcept
{
    return std::format("[w={}, h={}, d={}]", size3D.width, size3D.height, size3D.depth);
}
