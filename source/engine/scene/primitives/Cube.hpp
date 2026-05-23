#pragma once

#include <optional>
#include <vector>
#include "scene/Geometry.hpp"

namespace ptvc
{
    class Cube : public Geometry
    {
    public:
        constexpr static std::string_view sId = "cube";

        /**
         * @param a Side length (default 1.0f)
         */
        explicit Cube(std::optional<float> a = std::nullopt);

    private:
        static std::vector<Vertex>        sCubeVertices;
        static std::vector<std::uint32_t> sCubeIndices;
    };
}
