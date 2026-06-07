#include "Physics.hpp"

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <spdlog/spdlog.h>

#include <cstdarg>

using namespace JPH;

namespace ptvc {

namespace {
BroadPhaseLayer constexpr STATIC_BPL(0);
BroadPhaseLayer constexpr DYNAMIC_BPL(1);

// Categorize objects into broad phase layers to optimize collision detection
class BroadPhaseLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
  BroadPhaseLayerInterfaceImpl()
  {
    mObjectToBroadPhase[Physics::ObjectLayer::Static]  = STATIC_BPL;
    mObjectToBroadPhase[Physics::ObjectLayer::Dynamic] = DYNAMIC_BPL;
  }

  uint GetNumBroadPhaseLayers() const override { return Physics::ObjectLayer::Count; }

  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override
  {
    JPH_ASSERT(layer < Physics::ObjectLayer::Count);
    return mObjectToBroadPhase[layer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
  {
    switch((BroadPhaseLayer::Type)inLayer)
    {
      case(BroadPhaseLayer::Type)STATIC_BPL:
        return "STATIC";
      case(BroadPhaseLayer::Type)DYNAMIC_BPL:
        return "DYNAMIC";
      default:
        JPH_ASSERT(false);
        return "INVALID";
    }
  }
#endif

private:
  BroadPhaseLayer mObjectToBroadPhase[Physics::ObjectLayer::Count];
};

// Decide if collision check should be attempted between object and broad phase layer
class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter
{
public:
  bool ShouldCollide(ObjectLayer layer1, BroadPhaseLayer layer2) const override
  {
    switch(layer1)
    {
      case Physics::ObjectLayer::Static:
        return layer2 == DYNAMIC_BPL;
      case Physics::ObjectLayer::Dynamic:
        return true;
      default:
        JPH_ASSERT(false);
        return false;
    }
  }
};

// Decide if collision check happens between two objects
class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter
{
public:
  bool ShouldCollide(ObjectLayer object1, ObjectLayer object2) const override
  {
    switch(object1)
    {
      case Physics::ObjectLayer::Static:
        return object2 == Physics::ObjectLayer::Dynamic;
      case Physics::ObjectLayer::Dynamic:
        return true;
      default:
        JPH_ASSERT(false);
        return false;
    }
  }
};

// Trace logging
void TraceImpl(const char* inFMT, ...)
{
  va_list list;
  va_start(list, inFMT);
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), inFMT, list);
  va_end(list);
  spdlog::trace("Jolt: {}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
// Assert error logging
bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
  spdlog::error("Jolt assert: {}:{} ({}) {}", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
  return true;
}
#endif
}  // namespace

// Physics implementation
struct Physics::Impl
{
  JPH::TempAllocatorImpl            tempAllocator;
  JPH::JobSystemThreadPool          jobSystem;
  BroadPhaseLayerInterfaceImpl      bpLayerInterface;
  ObjectVsBroadPhaseLayerFilterImpl objectVsBPLayerFilter;
  ObjectLayerPairFilterImpl         layerPairFilter;
  JPH::PhysicsSystem                physicsSystem;

  Impl()
      : tempAllocator(10 * 1024 * 1024)
      , jobSystem(cMaxPhysicsJobs, cMaxPhysicsBarriers, static_cast<int>(std::thread::hardware_concurrency()) - 1)
  {
    physicsSystem.Init(65536, 0, 65536, 10240, bpLayerInterface, objectVsBPLayerFilter, layerPairFilter);
  }
};

Physics::Physics()
{
  RegisterDefaultAllocator();
  Trace = TraceImpl;
  JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl);

  Factory::sInstance = new Factory();
  RegisterTypes();

  mImpl = makeUnique<Impl>();
  mImpl->physicsSystem.OptimizeBroadPhase();
}

Physics::~Physics()
{
  mImpl.reset();

  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
}

void Physics::update(float deltaTime)
{
  mImpl->physicsSystem.Update(deltaTime, 1, &mImpl->tempAllocator, &mImpl->jobSystem);
}

JPH::BodyInterface& Physics::getBodyInterface() noexcept
{
  return mImpl->physicsSystem.GetBodyInterface();
}

JPH::PhysicsSystem& Physics::getPhysicsSystem() noexcept
{
  return mImpl->physicsSystem;
}

}  // namespace ptvc
