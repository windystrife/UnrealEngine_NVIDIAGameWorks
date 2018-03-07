// Copyright 2017 Google Inc.


#include "gvr_arm_model.h"
#include "GoogleVRController.h"

using namespace gvr_arm_model;

// Since Vector3 is often passed by reference, we need to include definitions for these constexpr.
constexpr const Vector3 Controller::FORWARD;
constexpr const Vector3 Controller::UP;
constexpr const Vector3 Controller::POINTER_OFFSET;
constexpr const Vector3 Controller::DEFAULT_SHOULDER_RIGHT;
constexpr const Vector3 Controller::ELBOW_MIN_RANGE;
constexpr const Vector3 Controller::ELBOW_MAX_RANGE;
constexpr const Vector3 Controller::NECK_OFFSET;

Controller::Controller()
: added_elbow_height(0.0f)
, added_elbow_depth(0.0f)
, pointer_tilt_angle(15.0f)
, follow_gaze(GazeBehavior::DuringMotion)
, handedness(Handedness::Right)
, use_accelerometer(false)
, fade_distance_from_face(0.32f)
, tooltip_min_distance_from_face(0.45f)
, tooltip_max_angle_from_camera(80)
, elbow_offset()
, zero_accel(0.0f, GRAVITY_FORCE, 0.0f)
, controller_alpha_value(1.0f)
, tooltip_alpha_value(0.0f)
, first_update(true){
  UpdateHandedness();
}

const Vector3& Controller::GetControllerPosition() const {
  return wrist_position;
}

const Quaternion& Controller::GetControllerRotation() const {
  return wrist_rotation;
}

const Vector3& Controller::GetPointerPositionOffset() const {
  return POINTER_OFFSET;
}

float Controller::GetAddedElbowHeight() const {
  return added_elbow_height;
}

void Controller::SetAddedElbowHeight(float elbow_height) {
  added_elbow_height = elbow_height;
}

float Controller::GetAddedElbowDepth() const {
  return added_elbow_depth;
}

void Controller::SetAddedElbowDepth(float elbow_depth) {
  added_elbow_depth = elbow_depth;
}

float Controller::GetPointerTiltAngle() {
  return pointer_tilt_angle;
}

void Controller::SetPointerTiltAngle(float tilt_angle) {
  pointer_tilt_angle = tilt_angle;
}

gvr_arm_model::Controller::GazeBehavior Controller::GetGazeBehavior() const {
  return follow_gaze;
}

void Controller::SetGazeBehavior(GazeBehavior gaze_behavior) {
  follow_gaze = gaze_behavior;
}

gvr_arm_model::Controller::Handedness Controller::GetHandedness() const {
  return handedness;
}

void Controller::SetHandedness(Handedness new_handedness) {
  handedness = new_handedness;
}

bool Controller::GetUseAccelerometer() const {
  return use_accelerometer;
}

void Controller::SetUseAccelerometer(bool new_use_accelerometer) {
  use_accelerometer = new_use_accelerometer;
}

float Controller::GetFadeDistanceFromFace() const {
  return fade_distance_from_face;
}

void Controller::SetFadeDistanceFromFace(float distance_from_face) {
  fade_distance_from_face = distance_from_face;
}

float Controller::GetTooltipMinDistanceFromFace() const {
  return tooltip_min_distance_from_face;
}

void Controller::SetTooltipMinDistanceFromFace(float distance_from_face) {
  tooltip_min_distance_from_face = distance_from_face;
}

int Controller::GetTooltipMaxAngleFromCamera() const {
  return tooltip_max_angle_from_camera;
}

void Controller::SetTooltipMaxAngleFromCamera(int max_angle_from_camera) {
  if (max_angle_from_camera < 0) {
    max_angle_from_camera = 0;
  } else if (max_angle_from_camera > 180) {
    max_angle_from_camera = 180;
  }

  tooltip_max_angle_from_camera = max_angle_from_camera;
}

float Controller::GetControllerAlphaValue() const {
  return controller_alpha_value;
}

float Controller::GetTooltipAlphaValue() const {
  return tooltip_alpha_value;
}

void Controller::UpdateHandedness() {
  // Place the shoulder in anatomical positions based on the height and handedness.
  handed_multiplier.Set(0.0f, 1.0f, 1.0f);
  if (handedness == Handedness::Right) {
    handed_multiplier.x(1.0f);
  } else if (handedness == Handedness::Left) {
    handed_multiplier.x(-1.0f);
  }

  // Place the shoulder in anatomical positions based on the height and handedness.
  shoulder_rotation = Quaternion();
  shoulder_position = DEFAULT_SHOULDER_RIGHT;
  shoulder_position.Scale(handed_multiplier);
}

void Controller::Update(const UpdateData& update_data) {
  UpdateHandedness();
  UpdateTorsoDirection(update_data);

  if (update_data.connected) {
    UpdateFromController(update_data);
  } else {
    ResetState();
  }

  if (use_accelerometer) {
    UpdateVelocity(update_data);
    TransformElbow(update_data);
  } else {
    elbow_offset = Vector3();
  }

  ApplyArmModel(update_data);
  UpdateTransparency(update_data);
}

void Controller::UpdateTorsoDirection(const UpdateData& update_data) {
  // Ignore updates here if requested.
  if (follow_gaze == GazeBehavior::Never) {
    return;
  }

  // Determine the gaze direction horizontally.
  Vector3 head_direction = update_data.headDirection;
  head_direction.y(0.0f);
  head_direction = head_direction.Normalized();

  if (follow_gaze == GazeBehavior::Always) {
    torso_direction = head_direction;
  } else if (follow_gaze == GazeBehavior::DuringMotion) {
    float angular_velocity = update_data.gyro.Magnitude();
    float gaze_filter_strength = FMath::Clamp((angular_velocity - 0.2f) / 45.0f, 0.0f, 0.1f);
    torso_direction = Vector3::Slerp(torso_direction, head_direction, gaze_filter_strength);
  }

  // Rotate the fixed joints.
  Quaternion gaze_rotation = Quaternion::FromToRotation(FORWARD, torso_direction);
  shoulder_rotation = gaze_rotation;
  shoulder_position = gaze_rotation.Rotated(shoulder_position);
}

void Controller::UpdateFromController(const UpdateData& update_data) {
  // Get the orientation-adjusted acceleration.
  Vector3 Accel = update_data.orientation.Rotated(update_data.acceleration);

  // Very slowly calibrate gravity force out of acceleration.
  zero_accel = zero_accel * GRAVITY_CALIB_STRENGTH + Accel * (1.0f - GRAVITY_CALIB_STRENGTH);
  filtered_accel = Accel - zero_accel;

  // If no tracking history, reset the velocity.
  if (first_update) {
    filtered_velocity = Vector3();
    first_update = false;
  }

  // IMPORTANT: The accelerometer is not reliable at these low magnitudes
  // so ignore it to prevent drift.
  if (filtered_accel.Magnitude() < MIN_ACCEL) {
    // Suppress the acceleration.
    filtered_accel = Vector3();
    filtered_velocity *= 0.9f;
  } else {
    // If the velocity is decreasing, prevent snap-back by reducing deceleration.
    Vector3 new_velocity = filtered_velocity + filtered_accel * update_data.deltaTimeSeconds;
    if (new_velocity.MagnitudeSquared() < filtered_velocity.MagnitudeSquared()) {
      filtered_accel *= 0.5f;
    }
  }
}

void Controller::UpdateVelocity(const UpdateData& update_data) {
  // Update the filtered velocity.
  filtered_velocity = filtered_velocity + (filtered_accel * update_data.deltaTimeSeconds);
  filtered_velocity = filtered_velocity * VELOCITY_FILTER_SUPPRESS;
}

void Controller::TransformElbow(const UpdateData& update_data) {
  // Apply the filtered velocity to update the elbow offset position.
  if (use_accelerometer) {
    elbow_offset += filtered_velocity * update_data.deltaTimeSeconds;
    elbow_offset.x(FMath::Clamp(elbow_offset.x(), ELBOW_MIN_RANGE.x(), ELBOW_MAX_RANGE.x()));
    elbow_offset.y(FMath::Clamp(elbow_offset.y(), ELBOW_MIN_RANGE.y(), ELBOW_MAX_RANGE.y()));
    elbow_offset.z(FMath::Clamp(elbow_offset.z(), ELBOW_MIN_RANGE.z(), ELBOW_MAX_RANGE.z()));
  }
}

void Controller::ApplyArmModel(const UpdateData &update_data) {
  // Find the controller's orientation relative to the player
  Quaternion controller_orientation = update_data.orientation;
  controller_orientation = shoulder_rotation.Inverted() * controller_orientation;

  // Get the relative positions of the joints
  elbow_position = Vector3(0.195f, -0.5f + added_elbow_height, 0.075f + added_elbow_depth);
  elbow_position = Vector3::Scale(elbow_position, handed_multiplier) + elbow_offset;
  wrist_position = Vector3::Scale(Vector3(0.0f, 0.0f, -0.25f), handed_multiplier);
  Vector3 arm_extension_offset = Vector3::Scale(Vector3(-0.13f, 0.14f, -0.08f), handed_multiplier);

  // Extract just the x rotation angle
  Vector3 controller_forward = controller_orientation.Rotated(FORWARD);
  float x_angle = 90.0f - Vector3::AngleDegrees(controller_forward, UP);

  // Remove the z rotation from the controller
  Quaternion x_y_rotation = Quaternion::FromToRotation(FORWARD, controller_forward);

  // Offset the elbow by the extension
  const float MIN_EXTENSION_ANGLE = 7.0f;
  const float MAX_EXTENSION_ANGLE = 60.0f;
  float normalized_angle = (x_angle - MIN_EXTENSION_ANGLE) / (MAX_EXTENSION_ANGLE - MIN_EXTENSION_ANGLE);
  float extension_ratio = FMath::Clamp(normalized_angle, 0.0f, 1.0f);
  if (!use_accelerometer) {
    elbow_position += arm_extension_offset * extension_ratio;
  }

  // Calculate the lerp interpolation factor
  const float EXTENSION_WEIGHT = 0.4f;
  float total_angle = Quaternion::AngleDegrees(x_y_rotation, Quaternion());
  float lerp_suppresion = 1.0f - pow(total_angle / 180.0f, 6);
  float lerp_value = lerp_suppresion * (0.4f + 0.6f * extension_ratio * EXTENSION_WEIGHT);

  // Apply the absolute rotations to the joints
  Quaternion lerp_rotation = Quaternion::Lerp(Quaternion(), x_y_rotation, lerp_value);
  elbow_rotation = shoulder_rotation * lerp_rotation.Inverted() * controller_orientation;
  wrist_rotation = shoulder_rotation * controller_orientation;

  Vector3 headPosition = Vector3::Zero;
  // Get the head position.
  if (is_locked_to_head) {
    headPosition = ApplyInverseNeckModel(update_data);
  }

  // Determine the relative positions
  elbow_position = headPosition + shoulder_rotation.Rotated(elbow_position);
  wrist_position = elbow_position + elbow_rotation.Rotated(wrist_position);
}

// Apply inverse neck model to both transform the head position to
// the center of the head and account for the head's rotation
// so that the motion feels more natural.
Vector3 Controller::ApplyInverseNeckModel(const UpdateData& update_data) {
  Quaternion headRotation = update_data.orientation;
  Vector3 headPosition = update_data.headPosition;
  Vector3 rotatedNeckOffset = headRotation.Rotated(NECK_OFFSET) - Vector3{ 0.0f, NECK_OFFSET.y(), 0.0f };
  headPosition -= rotatedNeckOffset;

  return headPosition;
}

void Controller::SetIsLockedToHead(bool is_locked) {
  is_locked_to_head = is_locked;
}

bool Controller::GetIsLockedToHead() {
  return is_locked_to_head;
}

void Controller::UpdateTransparency(const UpdateData &update_data) {
  Vector3 wrist_relative_to_head = wrist_position - update_data.headPosition;
  // Determine how vertical the controller is pointing.
  float animationDelta = DELTA_ALPHA * update_data.deltaTimeSeconds;
  float distance_to_face = wrist_relative_to_head.Magnitude();
  if (distance_to_face < fade_distance_from_face) {
    controller_alpha_value = FMath::Clamp(controller_alpha_value - animationDelta, 0.0f, 1.0f);
  } else {
    controller_alpha_value = FMath::Clamp(controller_alpha_value + animationDelta, 0.0f, 1.0f);
  }

  Vector3 wrist_from_head = wrist_position.Normalized() * -1.0f;
  float dot = wrist_rotation.Rotated(UP).Dot(wrist_from_head);
  float min_dot = (tooltip_max_angle_from_camera - 90.0f) / -90.0f;
  if (distance_to_face < fade_distance_from_face || distance_to_face > tooltip_min_distance_from_face || dot < min_dot) {
    tooltip_alpha_value = FMath::Clamp(tooltip_alpha_value - animationDelta, 0.0f, 1.0f);
  } else {
    tooltip_alpha_value = FMath::Clamp(tooltip_alpha_value + animationDelta, 0.0f, 1.0f);
  }
}

void Controller::ResetState() {
  // We've lost contact, quickly reset the state.
  filtered_velocity = filtered_velocity * 0.5f;
  filtered_accel = filtered_accel * 0.5f;
  first_update = true;
}