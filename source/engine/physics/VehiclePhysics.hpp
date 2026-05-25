#pragma once

#include <array>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <lib/ptr.hpp>

namespace ptvc {

class Physics;

enum class EWheel : int
{
  LeftFront,
  RightFront,
  LeftRear,
  RightRear,
  Count
};

/**
 * Physics simulation of 4-wheel vehicle using Jolt Physics.
 *
 * Owns all Jolt bodies, constraints, and collision filtering.
 * The owning Vehicle game object calls applyInput() each frame
 * and then reads the resulting transform via getTransform().
 */
class VehiclePhysics
{
public:
  static constexpr int kWheelCount = static_cast<int>(EWheel::Count);

  // Vehicle body dimensions
  static constexpr float HALF_WIDTH  = 0.9f;
  static constexpr float HALF_HEIGHT = 0.2f;
  static constexpr float HALF_LENGTH = 2.0f;

  // Wheel dimensions
  static constexpr float HALF_WHEEL_WIDTH  = 0.05f;
  static constexpr float HALF_WHEEL_HEIGHT = 0.3f;
  static constexpr float HALF_WHEEL_TRAVEL = 0.5f;

  // Steering limits
  static constexpr float MAX_STEERING_ANGLE = 0.785398f;

  VehiclePhysics(Physics& physics, const glm::vec3& startPosition);
  ~VehiclePhysics();

  /** Apply steering angle (radians) and drive speed (rad/s) to the wheels. */
  void applyInput(float steeringAngle, float speed);

  /** Read current body transform from physics into glm types. */
  void getTransform(glm::vec3& outTranslate, glm::quat& outRotation) const;

  /** Return the car body world-space position. */
  [[nodiscard]] glm::vec3 getPosition() const;

private:
  static inline constexpr bool isFrontWheel(EWheel wheel)
  {
    return wheel == EWheel::LeftFront || wheel == EWheel::RightFront;
  }

  static inline constexpr bool isLeftWheel(EWheel wheel)
  {
    return wheel == EWheel::LeftFront || wheel == EWheel::LeftRear;
  }

  // Suspension motor
  static constexpr float SUSPENSION_FREQUENCY = 3.0f;
  static constexpr float SUSPENSION_DAMPING   = 2.0f;
  static constexpr float SUSPENSION_MAX_FORCE = 1.0e5f;

  // Wheel friction and density
  static constexpr float WHEEL_FRICTION = 3.0f;
  static constexpr float WHEEL_DENSITY  = 1.0e4f;

  // Steering motor
  static constexpr float STEERING_SPEED     = 10.0f;
  static constexpr float STEERING_DAMPING   = 1.0f;
  static constexpr float STEERING_MAX_FORCE = 1.0e6f;

  // Drive motor
  static constexpr float DRIVE_FREQUENCY = 2.0f;
  static constexpr float DRIVE_DAMPING   = 1.0f;
  static constexpr float DRIVE_MAX_FORCE = 0.5e4f;

  // Collision group
  static constexpr JPH::CollisionGroup::GroupID    GROUP_CAR_BODY = 0;
  static constexpr JPH::CollisionGroup::SubGroupID SUBGROUP_BODY  = 0;

  Physics&                                                 mPhysics;
  JPH::BodyID                                              mCarBodyId;
  std::array<JPH::BodyID, kWheelCount>                     mWheelBodyIds = {};
  std::array<JPH::Ref<JPH::SixDOFConstraint>, kWheelCount> mWheels       = {};
  JPH::Ref<JPH::GroupFilterTable>                          mGroupFilter;
};

}  // namespace ptvc
