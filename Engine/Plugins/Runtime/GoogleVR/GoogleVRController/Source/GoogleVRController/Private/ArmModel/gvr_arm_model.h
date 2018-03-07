// Copyright 2017 Google Inc.

#pragma once

#include "gvr_arm_model_math.h"

namespace gvr_arm_model {
  class Controller {
  public:

    struct UpdateData {
      bool connected;
      Vector3 acceleration;
      Quaternion orientation;
      Vector3 gyro;
      Vector3 headDirection;
      Vector3 headPosition;
      float deltaTimeSeconds;
    };

    enum GazeBehavior {
      Never,
      DuringMotion,
      Always
    };

    enum Handedness {
      Right,
      Left,
      Unknown
    };

    Controller();

    // Returns the position of the controller relative to the head in gvr head space coordinates.
    // Negative Z is Forward, UE.
    // Positive X is Right, UE.
    // Positive Y is Up, UE.
    const Vector3& GetControllerPosition() const;

    // Returns the orientation of the controller relative to the head in gvr head space coordinates.
    // Negative Z is Forward, UE.
    // Positive X is Right, UE.
    // Positive Y is Up, UE.
    const Quaternion& GetControllerRotation() const;

    // Returns the offset of the pointer relative to the controller.
    const Vector3& GetPointerPositionOffset() const;

    // Height of the elbow.
    float GetAddedElbowHeight() const;
    void SetAddedElbowHeight(float elbow_height);

    // Depth of the elbow.
    float GetAddedElbowDepth() const;
    void SetAddedElbowDepth(float elbow_depth);

    // The Downward tilt or pitch of the laser pointer relative to the controller (degrees).
    float GetPointerTiltAngle();
    void SetPointerTiltAngle(float tilt_angle);

    // Determines how the shoulder should follow the gaze.
    GazeBehavior GetGazeBehavior() const;
    void SetGazeBehavior(GazeBehavior gaze_behavior);

    // Determines if the controller is in the left hand or right hand.
    Handedness GetHandedness() const;
    void SetHandedness(Handedness new_handedness);

    // Determines if the accelerometer should be used.
    bool GetUseAccelerometer() const;
    void SetUseAccelerometer(bool new_use_accelerometer);

    // Controller distance from the face after which the controller disappears.
    float GetFadeDistanceFromFace() const;
    void SetFadeDistanceFromFace(float distance_from_face);

    // Controller distance from face after which the tooltip appears.
    float GetTooltipMinDistanceFromFace() const;
    void SetTooltipMinDistanceFromFace(float distance_from_face);

    // When the angle (degrees) between the controller and the head is larger than
    // this value, the tooltip disappears.
    // If the value is 180, then the tooltips are always shown.
    // If the value is 90, the tooltips are only shown when they are facing the camera.
    int GetTooltipMaxAngleFromCamera() const;
    void SetTooltipMaxAngleFromCamera(int max_angle_from_camera);

    // Returns the alpha value that the controller should be rendered at.
    // This Prevents the controller from clipping with the camera.
    float GetControllerAlphaValue() const;

    // Returns the alpha value that the tooltips should be rendered at.
    float GetTooltipAlphaValue() const;

    // Wether or not the Arm Model is locked to the Head Pose
    void SetIsLockedToHead(bool is_locked);
    bool GetIsLockedToHead();

    void Update(const UpdateData& update_data);

  private:

    void UpdateHandedness();
    void UpdateTorsoDirection(const UpdateData& update_data);
    void UpdateFromController(const UpdateData& update_data);
    void UpdateVelocity(const UpdateData& update_data);
    void TransformElbow(const UpdateData& update_data);
    void ApplyArmModel(const UpdateData& update_data);
    Vector3 ApplyInverseNeckModel(const UpdateData& update_data);
    void UpdateTransparency(const UpdateData& update_data);

    void ResetState();

    float added_elbow_height;
    float added_elbow_depth;
    float pointer_tilt_angle;
    GazeBehavior follow_gaze;
    Handedness handedness;
    bool use_accelerometer;
    float fade_distance_from_face;
    float tooltip_min_distance_from_face;
    int tooltip_max_angle_from_camera;
    bool is_locked_to_head;

    Vector3 wrist_position;
    Quaternion wrist_rotation;

    Vector3 elbow_position;
    Quaternion elbow_rotation;

    Vector3 shoulder_position;
    Quaternion shoulder_rotation;

    Vector3 elbow_offset;
    Vector3 torso_direction;
    Vector3 filtered_velocity;
    Vector3 filtered_accel;
    Vector3 zero_accel;
    Vector3 handed_multiplier;
    float controller_alpha_value;
    float tooltip_alpha_value;

    bool first_update;

    // Strength of the acceleration filter (unitless).
    static constexpr float GRAVITY_CALIB_STRENGTH = 0.999f;

    // Strength of the velocity suppression (unitless).
    static constexpr float VELOCITY_FILTER_SUPPRESS = 0.99f;

    // The minimum allowable accelerometer reading before zeroing (m/s^2).
    static constexpr float MIN_ACCEL = 1.0f;

    // The expected force of gravity (m/s^2).
    static constexpr float GRAVITY_FORCE = 9.807f;

    // Amount of normalized alpha transparency to change per second.
    static constexpr float DELTA_ALPHA = 4.0f;

    // Unrotated Position offset from wrist to pointer.
    static constexpr Vector3 POINTER_OFFSET{ 0.0f, -0.009f, -0.109f };

    // Initial relative location of the shoulder (meters).
    static constexpr Vector3 DEFAULT_SHOULDER_RIGHT{ 0.19f, -0.19f, 0.03f };

    // The range of movement from the elbow position due to accelerometer (meters).
    static constexpr Vector3 ELBOW_MIN_RANGE{ -0.05f, -0.1f, -0.2f };
    static constexpr Vector3 ELBOW_MAX_RANGE{ 0.05f, 0.1f, 0.0f };

    // Forward Vector in GVR Space.
    static constexpr Vector3 FORWARD{ 0.0f, 0.0f, -1.0f };

    // Up Vector in GVR Space.
    static constexpr Vector3 UP{ 0.0f, 1.0f, 0.0f };

    // Position of the point between the eyes, relative to the neck pivot
    static constexpr Vector3 NECK_OFFSET{ 0, 0.075f, 0.08f };
  };
}