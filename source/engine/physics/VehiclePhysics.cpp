#include "VehiclePhysics.hpp"
#include "Physics.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <spdlog/spdlog.h>

using namespace JPH;
using EAxis = SixDOFConstraintSettings::EAxis;

namespace ptvc {

namespace {

// Wheel positions relative to the car body centre
const Vec3 kWheelPositions[] = {
    Vec3(-VehiclePhysics::HALF_WIDTH, -VehiclePhysics::HALF_HEIGHT, VehiclePhysics::HALF_LENGTH - 2.0f * VehiclePhysics::HALF_WHEEL_HEIGHT),
    Vec3(VehiclePhysics::HALF_WIDTH, -VehiclePhysics::HALF_HEIGHT, VehiclePhysics::HALF_LENGTH - 2.0f * VehiclePhysics::HALF_WHEEL_HEIGHT),
    Vec3(-VehiclePhysics::HALF_WIDTH, -VehiclePhysics::HALF_HEIGHT, -VehiclePhysics::HALF_LENGTH + 2.0f * VehiclePhysics::HALF_WHEEL_HEIGHT),
    Vec3(VehiclePhysics::HALF_WIDTH, -VehiclePhysics::HALF_HEIGHT, -VehiclePhysics::HALF_LENGTH + 2.0f * VehiclePhysics::HALF_WHEEL_HEIGHT),
};

}  // namespace

VehiclePhysics::VehiclePhysics(Physics& physics, const glm::vec3& startPosition)
    : mPhysics(physics)
{
  auto& bodyInterface = mPhysics.getBodyInterface();
  auto& physicsSystem = mPhysics.getPhysicsSystem();

  const RVec3 kStartPosition(startPosition.x, startPosition.y, startPosition.z);

  BoxShapeSettings bodyShapeSettings(Vec3(HALF_WIDTH, HALF_HEIGHT, HALF_LENGTH));
  bodyShapeSettings.SetDensity(1.5e3f);
  RefConst<Shape> bodyShape = bodyShapeSettings.Create().Get();

  // Filter table to prevent collisions between the car body and its own wheels
  mGroupFilter = new GroupFilterTable(kWheelCount + 1);

  Body* carBody = bodyInterface.CreateBody(BodyCreationSettings(bodyShape, kStartPosition, Quat::sIdentity(),
                                                                EMotionType::Dynamic, Physics::ObjectLayer::Dynamic));
  carBody->SetFriction(2.0f);
  carBody->SetCollisionGroup(CollisionGroup(mGroupFilter.GetPtr(), GROUP_CAR_BODY, SUBGROUP_BODY));
  bodyInterface.AddBody(carBody->GetID(), EActivation::Activate);
  mCarBodyId = carBody->GetID();

  Ref<CylinderShape> wheelShape = new CylinderShape(HALF_WHEEL_WIDTH, HALF_WHEEL_HEIGHT);
  wheelShape->SetDensity(WHEEL_DENSITY);

  for(int i = 0; i < kWheelCount; ++i)
  {
    const EWheel wheel   = static_cast<EWheel>(i);
    const bool   isFront = isFrontWheel(wheel);
    const bool   isLeft  = isLeftWheel(wheel);

    const RVec3 wheelPosTop    = kStartPosition + kWheelPositions[i];
    const RVec3 wheelPosBottom = wheelPosTop - Vec3(0.0f, HALF_WHEEL_TRAVEL, 0.0f);

    Body* wheelBody =
        bodyInterface.CreateBody(BodyCreationSettings(wheelShape, wheelPosBottom, Quat::sRotation(Vec3::sAxisZ(), 0.5f * JPH_PI),
                                                      EMotionType::Dynamic, Physics::ObjectLayer::Dynamic));
    wheelBody->SetFriction(WHEEL_FRICTION);
    wheelBody->SetCollisionGroup(
        CollisionGroup(mGroupFilter.GetPtr(), GROUP_CAR_BODY, static_cast<CollisionGroup::SubGroupID>(i + 1)));
    bodyInterface.AddBody(wheelBody->GetID(), EActivation::Activate);
    mWheelBodyIds[i] = wheelBody->GetID();

    // Six DOF constraint suspension
    SixDOFConstraintSettings settings;
    settings.mPosition1 = wheelPosTop;
    settings.mPosition2 = wheelPosBottom;
    settings.mAxisX1 = settings.mAxisX2 = isLeft ? -Vec3::sAxisX() : Vec3::sAxisX();
    settings.mAxisY1 = settings.mAxisY2 = Vec3::sAxisY();

    // Lock X and Z translation; Y is the suspension spring
    settings.MakeFixedAxis(EAxis::TranslationX);
    settings.SetLimitedAxis(EAxis::TranslationY, -HALF_WHEEL_TRAVEL, HALF_WHEEL_TRAVEL);
    settings.MakeFixedAxis(EAxis::TranslationZ);
    settings.mMotorSettings[EAxis::TranslationY] =
        MotorSettings(SUSPENSION_FREQUENCY, SUSPENSION_DAMPING, SUSPENSION_MAX_FORCE, 0.0f);

    // Steering for front wheels only
    if(isFront)
      settings.SetLimitedAxis(EAxis::RotationY, -MAX_STEERING_ANGLE, MAX_STEERING_ANGLE);
    else
      settings.MakeFixedAxis(EAxis::RotationY);

    // Lock Z rotation (prevents leaning)
    settings.MakeFixedAxis(EAxis::RotationZ);

    // Free X rotation (wheel spin) with a motor for driving
    settings.MakeFreeAxis(EAxis::RotationX);
    settings.mMotorSettings[EAxis::RotationX] = MotorSettings(DRIVE_FREQUENCY, DRIVE_DAMPING, 0.0f, DRIVE_MAX_FORCE);

    // Steering motors
    if(isFront)
    {
      settings.mMotorSettings[EAxis::RotationY] = MotorSettings(STEERING_SPEED, STEERING_DAMPING, 0.0f, STEERING_MAX_FORCE);
      settings.mMotorSettings[EAxis::RotationZ] = MotorSettings(STEERING_SPEED, STEERING_DAMPING, 0.0f, STEERING_MAX_FORCE);
    }

    // Create and register the constraint
    SixDOFConstraint* wheelConstraint = static_cast<SixDOFConstraint*>(settings.Create(*carBody, *wheelBody));
    physicsSystem.AddConstraint(wheelConstraint);
    mWheels[i] = wheelConstraint;

    // Initial suspension state
    wheelConstraint->SetTargetPositionCS(Vec3(0.0f, -HALF_WHEEL_TRAVEL, 0.0f));
    wheelConstraint->SetMotorState(EAxis::TranslationY, EMotorState::Position);

    if(isFront)
    {
      wheelConstraint->SetTargetOrientationCS(Quat::sIdentity());
      wheelConstraint->SetMotorState(EAxis::RotationY, EMotorState::Position);
      wheelConstraint->SetMotorState(EAxis::RotationZ, EMotorState::Position);
    }
  }
}

VehiclePhysics::~VehiclePhysics()
{
  auto& bodyInterface = mPhysics.getBodyInterface();
  auto& physicsSystem = mPhysics.getPhysicsSystem();

  for(int i = 0; i < kWheelCount; ++i)
  {
    if(mWheels[i] != nullptr)
    {
      physicsSystem.RemoveConstraint(mWheels[i].GetPtr());
      mWheels[i] = nullptr;
    }
  }

  for(int i = 0; i < kWheelCount; ++i)
  {
    if(!mWheelBodyIds[i].IsInvalid())
    {
      bodyInterface.RemoveBody(mWheelBodyIds[i]);
      bodyInterface.DestroyBody(mWheelBodyIds[i]);
      mWheelBodyIds[i] = BodyID();
    }
  }

  if(!mCarBodyId.IsInvalid())
  {
    bodyInterface.RemoveBody(mCarBodyId);
    bodyInterface.DestroyBody(mCarBodyId);
    mCarBodyId = BodyID();
  }
}

void VehiclePhysics::applyInput(const float steeringAngle, const float speed)
{
  auto& bodyInterface = mPhysics.getBodyInterface();

  // Wake the car body whenever input is given
  if(steeringAngle != 0.0f || speed != 0.0f)
  {
    bodyInterface.ActivateBody(mCarBodyId);
  }

  // Determine whether we should brake
  const Vec3  linearVelocity = bodyInterface.GetLinearVelocity(mCarBodyId);
  const Quat  bodyRotation   = bodyInterface.GetRotation(mCarBodyId);
  const float carSpeed       = linearVelocity.Dot(bodyRotation.RotateAxisZ());
  const bool  brake          = speed != 0.0f && carSpeed != 0.0f && Sign(speed) != Sign(carSpeed);

  auto setWheelDrive = [&](int idx) {
    SixDOFConstraint* wc = mWheels[idx];
    if(!wc)
      return;

    if(brake)
    {
      wc->SetTargetAngularVelocityCS(Vec3::sZero());
      wc->SetMotorState(EAxis::RotationX, EMotorState::Velocity);
    }
    else if(speed != 0.0f)
    {
      const float wheelSpeed = isLeftWheel(static_cast<EWheel>(idx)) ? -speed : speed;
      wc->SetTargetAngularVelocityCS(Vec3(wheelSpeed, 0.0f, 0.0f));
      wc->SetMotorState(EAxis::RotationX, EMotorState::Velocity);
    }
    else
    {
      wc->SetMotorState(EAxis::RotationX, EMotorState::Off);
    }
  };

  // Front wheels: steering + drive
  for(EWheel w : {EWheel::LeftFront, EWheel::RightFront})
  {
    const int         idx = static_cast<int>(w);
    SixDOFConstraint* wc  = mWheels[idx];
    if(!wc)
      continue;

    wc->SetTargetOrientationCS(Quat::sRotation(Vec3::sAxisY(), steeringAngle));
    setWheelDrive(idx);
  }

  // Rear wheels: drive only
  for(EWheel w : {EWheel::LeftRear, EWheel::RightRear})
  {
    setWheelDrive(static_cast<int>(w));
  }
}

void VehiclePhysics::getTransform(glm::vec3& outTranslate, glm::quat& outRotation) const
{
  if(mCarBodyId.IsInvalid())
    return;

  auto& bodyInterface = mPhysics.getBodyInterface();

  const RVec3 position = bodyInterface.GetPosition(mCarBodyId);
  const Quat  rotation = bodyInterface.GetRotation(mCarBodyId);

  outTranslate = glm::vec3(static_cast<float>(position.GetX()), static_cast<float>(position.GetY()),
                           static_cast<float>(position.GetZ()));

  outRotation = glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());
}

glm::vec3 VehiclePhysics::getPosition() const
{
  if(mCarBodyId.IsInvalid())
    return glm::vec3(0.0f);

  auto&       bodyInterface = mPhysics.getBodyInterface();
  const RVec3 position      = bodyInterface.GetPosition(mCarBodyId);

  return glm::vec3(static_cast<float>(position.GetX()), static_cast<float>(position.GetY()), static_cast<float>(position.GetZ()));
}

}  // namespace ptvc
