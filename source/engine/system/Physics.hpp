#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <lib/ptr.hpp>

class Physics
{
public:
  Physics();
  ~Physics();

  void update(float deltaTime);

  enum ObjectLayer : JPH::ObjectLayer
  {
    Static  = 0,
    Dynamic = 1,
    Count   = 2,
  };

private:
  struct Impl;
  UPtr<Impl> mImpl;
};
