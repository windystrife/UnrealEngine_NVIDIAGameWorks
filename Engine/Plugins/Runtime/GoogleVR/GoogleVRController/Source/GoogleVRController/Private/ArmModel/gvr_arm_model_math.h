// Copyright 2017 Google Inc.

#pragma once

/* This file exists to provide the minimal amount of common mathemtically utilities
*  that are needed for use by the Arm Model.
*  This is used instead of any engine-specific math library because this will soon be pulled out
*  into a pre-compiled library that is used by multiple engines to allow for code re-use.
*/
namespace gvr_arm_model {

  class Util {
  public:
    static float ToDegrees(float radians);
    static float ToRadians(float degrees);
  };

  class Vector3 {
  public:
    Vector3();

    constexpr Vector3(float x, float y, float z)
      : c_{ x, y, z }
    {}

    void x(float v);
    void y(float v);
    void z(float v);
    float x() const;
    float y() const;
    float z() const;

    Vector3& operator*=(float k);
    Vector3 operator*(float k) const;
    Vector3& operator+=(const Vector3& b);
    Vector3 operator+(const Vector3& b) const;
    Vector3& operator-=(const Vector3& b);
    Vector3 operator-(const Vector3& b) const;

    void Set(float x, float y, float z);
    void Scale(const Vector3& b);

    float Dot(const Vector3& b) const;
    Vector3 Cross(const Vector3& b) const;

    Vector3 Normalized();
    float MagnitudeSquared() const;
    float Magnitude() const;

    // Spherical Linear Interpolation.
    static Vector3 Slerp(Vector3 start, Vector3 end, float percent);
    static Vector3 Scale(const Vector3& a, const Vector3& b);
    static float AngleDegrees(const Vector3& a, const Vector3& b);

    const static Vector3 Zero;

  private:
    float c_[3];
  };

  class Quaternion {
  public:
    // Default constructor: Make no rotation
    Quaternion();

    // Constructor for explicitly specifying all elements
    Quaternion(const float& a, const float& b, const float& c, const float& d);

    float w() const;
    float x() const;
    float y() const;
    float z() const;

    Quaternion& operator+=(const Quaternion& q);
    Quaternion operator+(const Quaternion& q) const;
    Quaternion& operator-=(const Quaternion& q);
    Quaternion operator-(const Quaternion& q) const;
    Quaternion& operator*=(const Quaternion& q);
    Quaternion operator*(const Quaternion& q) const;
    Quaternion& operator*=(const float& k);
    Quaternion operator*(const float& k) const;
    const float& operator[](int index) const;
    float& operator[](int index);

    // Explicitly set all elements
    void Set(const float& a, const float& b,
      const float& c, const float& d);

    float Dot(const Quaternion& q) const;

    // Rotate a 3 Vector by the rotation represented by the quaternion in place.
    // Note: The quaternion must be a unit quaternion.
    void Rotate(Vector3* p_v) const;

    // Rotate a 3 Vector by the quaternion rotation returning a Vector object.
    // Note: The quaternion must be a unit quaternion.  Be aware that
    //       the return will perform an additional copy of a temporary object.
    Vector3 Rotated(const Vector3& v) const;

    Quaternion Normalized();
    Quaternion Inverted() const;

    float Norm() const;
    float NormSquared() const;

    static Quaternion FromToRotation(const Vector3& from_direction, const Vector3& to_direction);
    static Quaternion AxisAngle(const Vector3& axis, float angle);
    static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);
    static float AngleDegrees(const Quaternion& a, const Quaternion& b);

  private:
    float q_[4];
  };
}