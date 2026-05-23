#include "IO.hpp"

#include "lib/lib.hpp"

namespace ptvc::io
{
    void LoadedTextureData::free() const noexcept
    {
        stbi_image_free(pixels);
    }

    LoadedTextureData loadTextureFromFile(const std::filesystem::path& filePath, const int32_t desiredChannels) noexcept
    {
        if (!std::filesystem::exists(filePath))
        {
            exitWithError("The specified file {} does not exist.", filePath.string());
        }

        LoadedTextureData data = {};

        // .string() first or else .c_str() returns "const wchar_t*" on Windows
        data.pixels = stbi_load(filePath.string().c_str(), &data.width, &data.height, &data.channels, desiredChannels);
        if (!data.pixels)
        {
            exitWithError("Failed to load texture file: {}", filePath.string());
        }

        data.fileName = filePath.stem().filename().string();
        return data;
    }
}
