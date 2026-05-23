#pragma once

#include <string_view>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace ptvc::rhi
{
    // Forward Declarations
    class Device;

    // Describes a shader to be loaded from a file and compiled.
    struct ShaderInfo
    {
        std::string_view        filePath;
        vk::ShaderStageFlagBits shaderStage;
        std::string_view        entryPoint = "main";
    };

    // Compiled Shader Module and initial information.
    struct CompiledShader
    {
        ShaderInfo                        shaderInfo;
        vk::ShaderModule                  shaderModule;
        vk::PipelineShaderStageCreateInfo shaderStageInfo;
    };

    class Shaders
    {
    public:
        /**
         * Compile shaders with "shaderc" then create the Vulkan shader modules along with a filled in
         * vk::PipelineShaderStageCreateInfo struct. (Supports both GLSL and HLSL)
         * @param shaderInfos List of ShaderInfos
         * @param pDevice
         * @return List of compiled shaders
         */
        [[nodiscard]] static std::vector<CompiledShader> createShaderModules(const std::vector<ShaderInfo>& shaderInfos, const Device* pDevice) noexcept;
    };
}

