#include "Shaders.hpp"

#include <filesystem>
#include <fstream>
#include <ranges>
#include <regex>
#include <string>

#include <shaderc/shaderc.hpp>
#include <spdlog/spdlog.h>

#include "Device.hpp"
#include "lib/lib.hpp"

namespace ptvc::rhi
{
    namespace detail
    {
        // Read GLSL text file into a string, used for compilation with "shaderc".
        [[nodiscard]] static std::string readShaderFile(const std::string_view& filePath) noexcept
        {
            std::ifstream file(filePath.data());
            if (!file.is_open())
            {
                exitWithError("Failed to open file: {}", filePath);
            }

            std::vector<std::string> buffer;
            std::string line;
            while (std::getline(file, line))
            {
                buffer.push_back(std::format("{}\n", line));
            }

            file.close();
            return join(buffer, "");
        }

        // Convert the specified Vulkan shader stage to "shaderc" shader kind used for compilation
        [[nodiscard]] static shaderc_shader_kind getShaderKind(const ShaderInfo& shaderInfo) noexcept
        {
            using enum vk::ShaderStageFlagBits;
            switch (shaderInfo.shaderStage)
            {
                case eVertex:                   return shaderc_vertex_shader;
                case eTessellationControl:      return shaderc_tess_control_shader;
                case eTessellationEvaluation:   return shaderc_tess_evaluation_shader;
                case eGeometry:                 return shaderc_geometry_shader;
                case eFragment:                 return shaderc_fragment_shader;
                case eCompute:                  return shaderc_compute_shader;
                case eRaygenKHR:                return shaderc_raygen_shader;
                case eAnyHitKHR:                return shaderc_anyhit_shader;
                case eClosestHitKHR:            return shaderc_closesthit_shader;
                case eMissKHR:                  return shaderc_miss_shader;
                case eIntersectionKHR:          return shaderc_intersection_shader;
                case eCallableKHR:              return shaderc_callable_shader;
                case eTaskEXT:                  return shaderc_task_shader;
                case eMeshEXT:                  return shaderc_mesh_shader;
                default: {
                    exitWithError("Unknown shader kind for stage: {}", vk::to_string(shaderInfo.shaderStage));
                }
            }
        }

        // "Detect" shader language by heuristics, see function body for details.
        [[nodiscard]] static shaderc_source_language getShaderLanguage(const ShaderInfo& shaderInfo, const std::string& shaderSrc) noexcept
        {
            // By file extension (file must end in either .glsl or .hlsl)
            const auto shaderPath = std::filesystem::path(shaderInfo.filePath);
            const auto extension  = shaderPath.extension();
            if (extension == ".glsl")
            {
                return shaderc_source_language_glsl;
            }
            if (extension == ".hlsl")
            {
                return shaderc_source_language_hlsl;
            }

            // By File Content
            // Check for GLSL "#version xxx" directive
            static std::regex versionRegex(R"(#version\s+\d{3})");
            if (std::regex_search(shaderSrc, versionRegex))
            {
                return shaderc_source_language_glsl;
            }

            // Check for HLSL semantics
            static std::array<std::string_view, 8> hlslSemantics = {
                "SV_Position", "SV_Target0", "SV_Depth", "SV_VertexID", ": SV_", ": POSITION", ": NORMAL", ": COLOR0"
            };
            for (const auto& systemValueSemantic : hlslSemantics)
            {
                if (shaderSrc.contains(systemValueSemantic))
                {
                    return shaderc_source_language_hlsl;
                }
            }

            // Default to GLSL
            return shaderc_source_language_glsl;
        }

        /**
         * Use the "shaderc" library included in the Vulkan SDK to compile shaders during runtime.
         * If compilation fails, the app will exit with the compilation error message.
         * @param shaderInfo shader to compile
         * @return SPIR-V Binary
         */
        [[nodiscard]] static std::vector<uint32_t> compileShader(const ShaderInfo& shaderInfo) noexcept
        {
            const auto shaderSrc = readShaderFile(shaderInfo.filePath);

            const shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            options.SetSourceLanguage(getShaderLanguage(shaderInfo, shaderSrc));
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);

            #ifndef NDEBUG
            options.SetGenerateDebugInfo();
            #endif

            const auto module = compiler.CompileGlslToSpv(shaderSrc.data(), getShaderKind(shaderInfo), shaderInfo.filePath.data(), options);
            if (module.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                exitWithError("Failed to compile shader ({}): {}", shaderInfo.filePath, module.GetErrorMessage());
            }

            return { module.cbegin(), module.cend() };
        }
    }

    std::vector<CompiledShader> Shaders::createShaderModules(const std::vector<ShaderInfo>& shaderInfos, const Device* pDevice) noexcept
    {
        if (!pDevice)
        {
            exitWithError("Device is null");
        }

        std::vector<CompiledShader> result;
        for (const auto& [i, shaderInfo] : enumerate(shaderInfos))
        {
            // Was a file specified at all?
            if (shaderInfo.filePath.empty())
            {
                exitWithError("ShaderInfo with index {} didn't specify filePath.", i);
            }

            // Does the specified file exist?
            if (!std::filesystem::exists(shaderInfo.filePath))
            {
                exitWithError("The specified shader file ({}) does not exist.", shaderInfo.filePath);
            }

            // Read and compile shader source file using shaderc
            const auto spv = detail::compileShader(shaderInfo);

            auto shaderModuleCreateInfo = vk::ShaderModuleCreateInfo()
                .setCodeSize(sizeof(uint32_t) * spv.size())
                .setPCode(spv.data());

            const auto shaderModule = pDevice->getHandle().createShaderModule(shaderModuleCreateInfo);
            const CompiledShader compiledShader = {
                .shaderInfo      = shaderInfo,
                .shaderModule    = shaderModule,
                .shaderStageInfo = vk::PipelineShaderStageCreateInfo()
                    .setStage(shaderInfo.shaderStage)
                    .setModule(shaderModule)
                    .setPName(shaderInfo.entryPoint.data()),
            };

            pDevice->setLabel<vk::ShaderModule>({
                .name   = shaderInfo.filePath,
                .handle = shaderModule,
            });
            result.push_back(compiledShader);

            spdlog::debug("Compiled shader: {}", styled(shaderInfo.filePath, fg(fmt::color::light_gray)));
        }
        return result;
    }
}
