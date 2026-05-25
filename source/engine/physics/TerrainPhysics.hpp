#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

#include <cstdint>
#include <vector>

namespace ptvc {

class Physics;

class TerrainPhysics
{
public:
  explicit TerrainPhysics(Physics& physics);
  ~TerrainPhysics();

  void createBody(const uint16_t* pixels, uint32_t width, uint32_t height);

  [[nodiscard]] JPH::BodyID getBodyID() const noexcept { return mBodyID; }

  [[nodiscard]] const JPH::HeightFieldShape* getShape() const noexcept { return mShape.GetPtr(); }

  [[nodiscard]] uint32_t getSampleCount() const noexcept { return mSampleCount; }

private:
  Physics&                        mPhysics;
  JPH::BodyID                     mBodyID;
  JPH::Ref<JPH::HeightFieldShape> mShape;

  std::vector<float> mHeightSamples;
  uint32_t           mSampleCount = 0;
};

}  // namespace ptvc
