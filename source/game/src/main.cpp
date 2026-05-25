#include <spdlog/spdlog.h>

#include <core/Application.hpp>
#include <scene/OrbitCamera.hpp>
#include "GameLayer.hpp"

int main()
{
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

  auto options = ptvc::ApplicationOptions{
      .windowOptions =
          {
              .size  = {1280, 720},
              .title = "Brumm Brumm Bau",
          },
      .vulkanOptions =
          {
              // Configure Vulkan extensions
              .extensions = [](ptvc::rhi::Extensions& extensions) -> void {
                using namespace ptvc::rhi;
                extensions.addPlatformRequiredExtensions()
                    .addExtension(vk::KHRSwapchainExtensionName, FeatureRequest::Required)
                    .addExtension(vk::KHRDeferredHostOperationsExtensionName, FeatureRequest::Required);
              },
          },
  };

  // Create Application
  const auto app = makeUnique<ptvc::Application>(options);

  // Set scene, register layers
  app->setScene<ptvc::Scene>();

  // Initialize an orbit camera that will follow the vehicle
  app->getScene()->initCamera<ptvc::OrbitCamera>(1280.0f / 720.0f);

  app->registerLayer<GameLayer>();

  // Start main loop
  app->run();

  return 0;
}
