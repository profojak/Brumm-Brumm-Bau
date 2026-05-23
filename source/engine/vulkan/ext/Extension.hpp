#pragma once

#include <functional>
#include <string>
#include <spdlog/fmt/bundled/color.h>

#include "VulkanCore.hpp"

namespace ptvc::rhi
{
    enum class FeatureRequest
    {
        Disabled = 0,
        Optional = 1,
        Required = 2,
    };

    [[nodiscard]] constexpr std::string toString(const FeatureRequest e) noexcept
    {
        using enum FeatureRequest;
        switch (e)
        {
            case Disabled: return "Disabled";
            case Optional: return "Optional";
            case Required: return "Required";
            default:       return "unknown";
        }
    }

    [[nodiscard]] constexpr fmt::color getColor(const FeatureRequest e) noexcept
    {
        using enum FeatureRequest;
        switch (e)
        {
            case Disabled:  return fmt::color::gray;
            case Optional:  return fmt::color::light_gray;
            case Required:  return fmt::color::cadet_blue;
            default:        return fmt::color::white;
        }
    }

    /**
     * @brief A class for working with Vulkan Extensions
     * @note - For extensions that only require their name to be specified it is enough to instantiate
     * this class with the two argument ctor.\n
     * - For extensions that require a "vk::PhysicalDevice...Features" struct create a subclass (see the macro "def_VulkanExt").
     */
    class Extension
    {
    public:
        explicit Extension(const char* extensionName, FeatureRequest requested);

        Extension(const char* extensionName, FeatureRequest requested, const std::function<void()>& structInitFn);

        virtual ~Extension() = default;

        void preCreateDevice(vk::DeviceCreateInfo& deviceCreateInfo) const noexcept;

        void setSupported(bool value) noexcept;

        [[nodiscard]] bool isActive() const noexcept;

        [[nodiscard]] const char* getName() const noexcept;

        [[nodiscard]] FeatureRequest getRequestType() const noexcept;

        [[nodiscard]] std::string toString(size_t width = 0) const noexcept;

    protected:
        void* mFeatureStructPtr = nullptr;

    private:
        struct VulkanAnyStruct
        {
            vk::StructureType sType;
            const void*       pNext;
        };

        FeatureRequest        mRequest             = FeatureRequest::Optional;
        bool                  mSupported           = false;
        const char*           mExtensionName       = nullptr;
        std::function<void()> mStructInitFn        = [](){};
    };

    /**
     * Define Vulkan Extensions that require a features struct using this macro.
     * @param NAME Extension Name (for the subclass)
     * @param STR_EXT_NAME Name of the extension as defined by the Vulkan specification.
     * @param STRUCT_T Type of the ...Features struct
     * @param FN Lambda function for initializing the feature struct
     */
    #define def_VulkanExt(NAME, STR_EXT_NAME, STRUCT_T, FN) \
        class NAME : public Extension {                     \
        public:                                             \
            NAME(const FeatureRequest requested)            \
            : Extension(STR_EXT_NAME, requested, FN) {      \
                mFeatureStructPtr = &mFeatureStruct;        \
            }                                               \
            ~NAME() override = default;                     \
        private:                                            \
            STRUCT_T mFeatureStruct;                        \
        }

    // ========================================
    // Vulkan Extensions
    // ========================================
    // VK_KHR_acceleration_structure
    def_VulkanExt(
        VulkanAccelerationStructureExt,
        vk::KHRAccelerationStructureExtensionName,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        [&]() -> void {
            mFeatureStruct = vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
                .setAccelerationStructure(true);
        }
    );

    // VK_KHR_ray_tracing_pipeline
    def_VulkanExt(
        VulkanRayTracingPipelineExt,
        vk::KHRRayTracingPipelineExtensionName,
        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
        [&]() -> void {
            mFeatureStruct = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR()
                .setRayTracingPipeline(true);
        }
    );

    // VK_KHR_ray_query
    def_VulkanExt(
        VulkanRayQueryExt,
        vk::KHRRayQueryExtensionName,
        vk::PhysicalDeviceRayQueryFeaturesKHR,
        [&]() -> void {
            mFeatureStruct = vk::PhysicalDeviceRayQueryFeaturesKHR()
                .setRayQuery(true);
        }
    );

    // VK_EXT_mesh_shader
    def_VulkanExt(
        VulkanMeshShaderExt,
        vk::EXTMeshShaderExtensionName,
        vk::PhysicalDeviceMeshShaderFeaturesEXT,
        [&]() -> void {
            mFeatureStruct = vk::PhysicalDeviceMeshShaderFeaturesEXT()
                .setMeshShader(true)
                .setTaskShader(true)
                .setMeshShaderQueries(true);
        }
    );
}

#undef def_VulkanExt
