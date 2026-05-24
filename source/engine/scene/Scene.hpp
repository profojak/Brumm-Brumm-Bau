#pragma once

#include <type_traits>
#include <vector>
#include <SDL3/SDL_events.h>

#include "Buffer.hpp"
#include "Descriptor.hpp"
#include "GameObject.hpp"
#include "ICamera.hpp"
#include "VulkanContext.hpp"
#include "vulkan/VulkanCore.hpp"

namespace ptvc {
enum SceneDescriptorBindings : uint32_t
{
  SceneDescriptorBindings_CameraUniform    = 0,
  SceneDescriptorBindings_DirectionalLight = 1,
  SceneDescriptorBindings_PointLight       = 2,
};

/**
 * Scene (base) class for managing cameras and game objects.
 */
class Scene
{
public:
  /**
   * Create a Scene
   * @param vulkanContext
   */
  explicit Scene(const SPtr<rhi::VulkanContext>& vulkanContext);

  virtual ~Scene() = default;

  /**
   * Add a new GameObject to the Scene.
   * @tparam T GameObject type
   * @param args Constructor arguments
   * @return Pointer to the new GameObject
   */
  template <class T = GameObject, class... Args>
    requires std::is_base_of_v<GameObject, T>
  T* addGameObject(Args&&... args) noexcept
  {
    mObjects.push_back(makeUnique<T>(std::forward<Args>(args)...));
    return static_cast<T*>(mObjects.back().get());
  }

  /**
   * Create and set the Camera for the Scene
   * @tparam T Camera type
   * @param args Constructor arguments
   * @return Pointer to the new Camera
   */
  template <class T, class... Args>
    requires std::is_base_of_v<ICamera, T>
  T* initCamera(Args&&... args) noexcept
  {
    mCamera = makeUnique<T>(std::forward<Args>(args)...);
    return static_cast<T*>(mCamera.get());
  }

  /**
   * Handle events related to cameras and game objects.
   * @param event
   */
  virtual void onEvent(const SDL_Event& event) noexcept;

  /**
   * Handle updates related to cameras and game objects.
   * @param deltaTime Delta time in seconds
   * @param frame Current frame
   */
  virtual void onUpdate(float deltaTime, const rhi::Frame& frame) noexcept;

  /**
   * @return Vector of GameObjects
   */
  [[nodiscard]] const std::vector<UPtr<GameObject>>& getGameObjects() const noexcept;

  /**
   * @return Scene Descriptor
   */
  [[nodiscard]] const SPtr<rhi::Descriptor>& getDescriptor() const noexcept;

private:
  // Create the Scene Descriptor and related resources (e.g. uniform buffers).
  void createSceneDescriptor() noexcept;

  SPtr<rhi::VulkanContext> mVulkanContext;

  UPtr<ICamera>                 mCamera;
  std::vector<UPtr<GameObject>> mObjects;

  // Test lights
  SPtr<rhi::Buffer> mPointLight;
  SPtr<rhi::Buffer> mDirectionalLight;

  /**
   * [Scene Descriptor]
   * - Descriptor Scene related resources with bindings defined in the
   * "SceneDescriptorBindings" enum at the top of this file.
   * - The number of sets is at least the number of frames in flight.
   */
  SPtr<rhi::Descriptor> mDescriptor;

  // Camera uniform buffers (count = no. descriptor sets)
  std::vector<SPtr<rhi::Buffer>> mCameraUniformBuffers;
};
}  // namespace ptvc
