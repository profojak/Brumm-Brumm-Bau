#include "TerrainPhysics.hpp"
#include "Physics.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <spdlog/spdlog.h>

namespace ptvc {

// Match the tessellation shader constants
namespace {
constexpr float HEIGHT_SCALE       = 12.0f;
constexpr float TERRAIN_WORLD_SIZE = 62.0f;
}  // namespace

TerrainPhysics::TerrainPhysics(Physics& physics)
    : mPhysics(physics)
{
}

TerrainPhysics::~TerrainPhysics()
{
  if(!mBodyID.IsInvalid())
  {
    mPhysics.getBodyInterface().RemoveBody(mBodyID);
    mPhysics.getBodyInterface().DestroyBody(mBodyID);
  }
}

void TerrainPhysics::createBody(const uint16_t* pixels, const uint32_t width, const uint32_t height)
{
  if(!pixels || width == 0 || height == 0)
  {
    spdlog::error("TerrainPhysics::createBody: Invalid parameters (pixels={}, width={}, height={})",
                  static_cast<const void*>(pixels), width, height);
    return;
  }

  mSampleCount = width;
  if(width != height)
  {
    spdlog::warn("TerrainPhysics: Heightmap is not square ({}x{}), using width ({}) as sample count", width, height, mSampleCount);
  }

  const uint32_t sampleCount = mSampleCount;
  mHeightSamples.resize(static_cast<size_t>(sampleCount) * sampleCount);
  for(uint32_t y = 0; y < sampleCount; y++)
  {
    for(uint32_t x = 0; x < sampleCount; x++)
    {
      const uint32_t pixelIndex       = y * sampleCount + x;
      const float    normalizedHeight = static_cast<float>(pixels[pixelIndex]) / 65535.0f;
      mHeightSamples[pixelIndex]      = normalizedHeight * HEIGHT_SCALE;
    }
  }

  // Compute cell size so heightfield matches rendered terrain extents
  const float     cellSize = TERRAIN_WORLD_SIZE / static_cast<float>(sampleCount - 1);
  const JPH::Vec3 offset(-TERRAIN_WORLD_SIZE * 0.5f, 0.0f, -TERRAIN_WORLD_SIZE * 0.5f);
  const JPH::Vec3 scale(cellSize, 1.0f, cellSize);

  JPH::HeightFieldShapeSettings settings(mHeightSamples.data(), offset, scale, sampleCount);
  settings.mBlockSize     = 4;
  settings.mBitsPerSample = 8;

  auto result = settings.Create();
  if(!result.IsValid())
  {
    spdlog::error("TerrainPhysics: Failed to create HeightFieldShape: {}", result.GetError());
    return;
  }

  mShape = JPH::StaticCast<JPH::HeightFieldShape>(result.Get());
  if(!mBodyID.IsInvalid())
  {
    mPhysics.getBodyInterface().RemoveBody(mBodyID);
    mPhysics.getBodyInterface().DestroyBody(mBodyID);
    mBodyID = JPH::BodyID();
  }

  // Create static body from heightfield shape
  JPH::BodyCreationSettings bodySettings(mShape, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                         Physics::ObjectLayer::Static);

  auto& bodyInterface = mPhysics.getBodyInterface();
  auto* body          = bodyInterface.CreateBody(bodySettings);
  mBodyID             = body->GetID();

  bodyInterface.AddBody(mBodyID, JPH::EActivation::DontActivate);
}

}  // namespace ptvc
