#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <Image.hpp>
#include <Buffer.hpp>
#include <Descriptor.hpp>
#include <VulkanContext.hpp>
#include <render/Pipeline.hpp>

namespace ptvc {

class Scene;
struct CameraData;
struct DirectionalLight;

/**
 * Manages shadow mapping for a single directional light (sun).
 * Renders the scene from the light's perspective into a depth texture,
 * then the main rendering samples it with percentage closer filtering (PCF).
 *
 * Artifact countermeasures:
 * - Depth bias (constant + slope-scaled) in the shadow pipeline
 * - Front-face culling in the shadow pass (renders back faces to avoid acne)
 * - Small per-sample bias in the PCF shader
 * - PCF softens shadow edges and hides aliasing
 */
class ShadowMap
{
public:
  /**
   * @param vulkanContext   The Vulkan context
   * @param sceneDescriptor The scene descriptor (set=0) for binding in shadow pipelines
   */
  ShadowMap(const SPtr<rhi::VulkanContext>& vulkanContext, const SPtr<rhi::Descriptor>& sceneDescriptor);

  ~ShadowMap();

  /**
   * Update the light-space view-projection matrix based on the camera frustum.
   * Should be called every frame before renderShadowPass().
   * @param cameraData Current camera data
   * @param sun        Directional light (sun) data
   */
  void updateLightSpace(const CameraData& cameraData, const DirectionalLight& sun) noexcept;

  /**
   * Render all scene geometry into the shadow map from the light's perspective.
   * @param frame  Current frame data
   * @param scene  The scene to render (terrain + game objects)
   */
  void renderShadowPass(const rhi::Frame& frame, Scene& scene) noexcept;

  /**
   * Create the terrain shadow pipeline.
   * Must be called after the terrain is created.
   * @param terrainDescriptor Descriptor for terrain resources (set=1)
   */
  void createTerrainShadowPipeline(const SPtr<rhi::Descriptor>& terrainDescriptor) noexcept;

  [[nodiscard]] vk::ImageView       getShadowMapView() const noexcept;
  [[nodiscard]] vk::Sampler         getShadowMapSampler() const noexcept;
  [[nodiscard]] SPtr<rhi::Buffer>   getLightSpaceUBO() const noexcept;
  [[nodiscard]] const vk::Extent2D& getShadowMapSize() const noexcept { return mSize; }

private:
  void createShadowResources() noexcept;
  void createObjectShadowPipeline() noexcept;

  SPtr<rhi::VulkanContext> mVulkanContext;
  SPtr<rhi::Descriptor>    mSceneDescriptor;

  // Shadow map image and sampler
  vk::Extent2D     mSize = {6144, 2048};  // 3 cascades
  SPtr<rhi::Image> mShadowMapImage;
  vk::Sampler      mShadowMapSampler = nullptr;
  bool             mFirstShadowPass  = true;
  glm::mat4        mCascadeLightVPs[3];

  // Light-space uniform buffer
  SPtr<rhi::Buffer> mLightSpaceUBO;
  struct alignas(16) LightSpaceData
  {
    glm::mat4 lightVP[3];
    glm::vec4 cascadeSplits;
  };

  // Pipelines for the shadow pass
  SPtr<rhi::Pipeline> mObjectShadowPipeline;
  SPtr<rhi::Pipeline> mTerrainShadowPipeline;
};

}  // namespace ptvc
