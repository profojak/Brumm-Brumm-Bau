#include <spdlog/spdlog.h>

#include <core/Application.hpp>
#include "GameLayer.hpp"

int main()
{
    spdlog::set_pattern("[%^%l%$] %v");

    auto options = ptvc::ApplicationOptions {
        .windowOptions = {
            .size  = { 1280, 720 },
            .title = "PTVC Framework : Example",
        },
        .vulkanOptions = {
            // Configure Vulkan extensions
            .extensions = [](ptvc::rhi::Extensions& extensions) -> void {
                using namespace ptvc::rhi;
                extensions
                    .addPlatformRequiredExtensions()
                    .addExtension(vk::KHRSwapchainExtensionName, FeatureRequest::Required)
                    .addExtension(vk::KHRDeferredHostOperationsExtensionName, FeatureRequest::Required)
                    .addExtension<VulkanAccelerationStructureExt>(FeatureRequest::Optional)
                    .addExtension<VulkanRayQueryExt>(FeatureRequest::Optional)
                    .addExtension<VulkanRayTracingPipelineExt>(FeatureRequest::Optional)
                    .addExtension<VulkanMeshShaderExt>(FeatureRequest::Optional);
            },
        },
    };

    // Create Application
    const auto app = makeUnique<ptvc::Application>(options);

    // Set scene, register layers
    app->setScene<ptvc::Scene>();
    app->registerLayer<GameLayer>();

    // Start main loop
    app->run();

    return 0;
}
