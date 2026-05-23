#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include <stb_image.h>

namespace ptvc::io
{
    /**
     * Struct containing pixel and metadata for textures loaded with
     * loadTextureFromFile and similar functions. The pixel data must be
     * freed manually using free().
     */
    struct LoadedTextureData
    {
        stbi_uc*    pixels;
        int32_t     width;
        int32_t     height;
        int32_t     channels;

        std::string fileName;

        // Free pixel data
        void free() const noexcept;
    };

    /**
     * Load a texture from a file on disk using "stb_image"
     * @param filePath
     * @param desiredChannels Which color channels to load (default: RGBA)
     * @return Pixel and Metadata
     */
    [[nodiscard]] LoadedTextureData loadTextureFromFile(
        const std::filesystem::path& filePath,
        int32_t                      desiredChannels = STBI_rgb_alpha
    ) noexcept;
}
