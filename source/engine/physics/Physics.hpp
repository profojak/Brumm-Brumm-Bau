#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <lib/ptr.hpp>


namespace ptvc {

class Physics
{
public:
  Physics();
  ~Physics();

  void update(float deltaTime);

  [[nodiscard]] JPH::BodyInterface& getBodyInterface() noexcept;
  [[nodiscard]] JPH::PhysicsSystem& getPhysicsSystem() noexcept;

  enum ObjectLayer : ::JPH::ObjectLayer
  {
    Static  = 0,
    Dynamic = 1,
    Count   = 2,
  };

private:
  struct Impl;
  UPtr<Impl> mImpl;
};

}  // namespace ptvc
