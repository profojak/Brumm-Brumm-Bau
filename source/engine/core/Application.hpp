#pragma once

#include <vector>

#include "ApplicationOptions.hpp"
#include "Layer.hpp"
#include "lib/ptr.hpp"
#include "scene/Scene.hpp"
#include "ui/ImGuiIntegration.hpp"
#include "vulkan/VulkanContext.hpp"
#include "wsi/sdl/SDLWindow.hpp"

namespace ptvc {
class Application
{
public:
  explicit Application(ApplicationOptions options);

  ~Application();

  // Main application loop
  void run();

  /**
         * Instantiate a Scene and set it as the current one.
         * @tparam TScene Scene class type
         */
  template <class TScene>
    requires(std::is_base_of_v<Scene, TScene>)
  void setScene() noexcept
  {
    mScene = makeUnique<TScene>(mVulkanContext);
  }

  /**
         * Add a Layer to the layer stack managed by the Application.
         * Only one instance of a specific layer can exist at a time.
         * @tparam TLayer Layer class type
         */
  template <class TLayer>
    requires(std::is_base_of_v<ILayer, TLayer>)
  void registerLayer() noexcept
  {
    if(getLayer<TLayer>() != nullptr)
    {
      exitWithError("The Application already has an instance of the Layer being registered");
    }
    mLayers.push_back(makeUnique<TLayer>());
  }

  /**
         * Get a pointer to the specified layer type.
         * @tparam TLayer Layer class type
         * @return Layer when present, nullptr otherwise
         */
  template <class TLayer>
    requires(std::is_base_of_v<ILayer, TLayer>)
  TLayer* getLayer() noexcept
  {
    for(const auto& layer : mLayers)
    {
      if(auto pLayer = dynamic_cast<TLayer*>(layer.get()))
      {
        return pLayer;
      }
    }
    return nullptr;
  }

  [[nodiscard]] Scene* getScene() const noexcept;

  [[nodiscard]] const SPtr<rhi::VulkanContext>& getVulkanContext() const noexcept;

  [[nodiscard]] static Application* getApplication() noexcept;

private:
  void init_UserInterface() noexcept;

  bool mRunning   = false;
  bool mMinimized = false;

  UPtr<Scene> mScene;

  class DebugLayer* mDebugLayer = nullptr;

  std::vector<UPtr<ILayer>> mLayers;

  UPtr<ui::ImGuiIntegration> mImGui;
  SPtr<rhi::VulkanContext>   mVulkanContext;
  SPtr<wsi::SDLWindow>       mWindow;
};
}  // namespace ptvc
