#pragma once

#include <glm/glm.hpp>

#include <scene/ICamera.hpp>
#include <vulkan/Buffer.hpp>
#include <vulkan/Descriptor.hpp>
#include <vulkan/Image.hpp>
#include <vulkan/VulkanContext.hpp>
#include <vulkan/render/Pipeline.hpp>

namespace ptvc {

class TerrainPhysics;

enum class DebugRenderMode : int32_t;

struct TerrainTessellationData
{
  float tessellationFactor;
};

class Terrain
{
public:
  Terrain(const SPtr<rhi::VulkanContext>& vulkanContext,
          const SPtr<rhi::Descriptor>&    sceneDescriptor,
          const SPtr<TerrainPhysics>&     terrainPhysics = nullptr);

  ~Terrain();

  void onRender(const rhi::Frame& frame, DebugRenderMode debugRenderMode) const noexcept;

  /**
   * Update tessellation uniform buffer with current camera data
   */
  void updateTessellationData(const CameraData& cameraData, float tessellationFactor) noexcept;

  [[nodiscard]] SPtr<rhi::Pipeline> getPipeline() const noexcept { return mPipeline; }

  [[nodiscard]] const SPtr<rhi::Image>& getHeightmapImage() const noexcept { return mHeightmapImage; }
  [[nodiscard]] vk::Sampler             getHeightmapSampler() const noexcept { return mHeightmapSampler; }

  [[nodiscard]] const SPtr<rhi::Image>& getTerrainTextureFront() const noexcept { return mTerrainTextureFront; }
  [[nodiscard]] const SPtr<rhi::Image>& getTerrainTextureSide() const noexcept { return mTerrainTextureSide; }
  [[nodiscard]] const SPtr<rhi::Image>& getTerrainTextureUp() const noexcept { return mTerrainTextureUp; }

private:
  void generateBaseMesh() noexcept;
  void loadHeightmap() noexcept;
  void loadTerrainTextures() noexcept;
  void createTerrainDescriptor() noexcept;
  void createPipeline() noexcept;
  void createWireframePipeline() noexcept;

  SPtr<rhi::VulkanContext> mVulkanContext;
  SPtr<rhi::Descriptor>    mSceneDescriptor;

  // Base mesh buffers
  SPtr<rhi::Buffer> mVertexBuffer;
  SPtr<rhi::Buffer> mIndexBuffer;
  uint32_t          mIndexCount = 0;

  // Heightmap texture
  SPtr<rhi::Image> mHeightmapImage;
  vk::Sampler      mHeightmapSampler = nullptr;

  // Triplanar terrain textures
  SPtr<rhi::Image> mTerrainTextureFront;
  SPtr<rhi::Image> mTerrainTextureSide;
  SPtr<rhi::Image> mTerrainTextureUp;
  vk::Sampler      mTerrainTextureSampler = nullptr;

  // Tessellation UBO
  TerrainTessellationData mTessellationData = {};
  SPtr<rhi::Buffer>       mTessellationUBO;

  // Terrain descriptor
  SPtr<rhi::Descriptor> mDescriptor;

  // Pipelines
  SPtr<rhi::Pipeline>  mPipeline;
  SPtr<rhi::Pipeline>  mWireframePipeline;
  SPtr<TerrainPhysics> mTerrainPhysics = nullptr;
};

}  // namespace ptvc
