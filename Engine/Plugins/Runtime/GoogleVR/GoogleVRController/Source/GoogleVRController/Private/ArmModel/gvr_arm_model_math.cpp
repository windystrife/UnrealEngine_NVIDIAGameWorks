// Copyright 2017 Google Inc.

#include "gvr_arm_model_math.h"
#include "gvr_arm_model.h"
#include "GoogleVRController.h"

using namespace gvr_arm_model;

/*
* Definitions for Util class
*/

float Util::ToDegrees(float radians) {
  return (radians * 180.0f) / PI;
}

float Util::ToRadians(float degrees) {
  return (degrees * PI) / 180.0f;
}

/*
* Definitions for Vector3 class
*/


Vector3::Vector3()
  : c_() {

}

void Vector3::x(float v) {
  c_[0] = v;
}

void Vector3::y(float v) {
  c_[1] = v;
}

void Vector3::z(float v) {
  c_[2] = v;
}

float Vector3::x() const {
  return c_[0];
}

float Vector3::y() const {
  return c_[1];
}

float Vector3::z() const {
  return c_[2];
}

const Vector3 Vector3::Zero = Vector3{ 0.0f, 0.0f, 0.0f };

Vector3& Vector3::operator*=(float k) {
  Set(x() * k, y() * k, z() * k);
  return *this;
}

Vector3 Vector3::operator*(float k) const {
  return Vector3(*this) *= k;
}

Vector3& Vector3::operator+=(const Vector3& b) {
  Set(x() + b.x(), y() + b.y(), z() + b.z());
  return *this;
}

Vector3 Vector3::operator+(const Vector3& b) const {
  return Vector3(*this) += b;
}

Vector3& Vector3::operator-=(const Vector3& b) {
  Set(x() - b.x(), y() - b.y(), z() - b.z());
  return *this;
}

Vector3 Vector3::operator-(const Vector3& b) const {
  return Vector3(*this) -= b;
}

void Vector3::Set(float x, float y, float z) {
  *this = Vector3(x, y, z);
}

void Vector3::Scale(const Vector3& b)
{
  *this = Vector3(x() * b.x(), y() * b.y(), z() * b.z());
}

Vector3 Vector3::Normalized() {
  float magnitude = Magnitude();
  if (magnitude != 0.0f) {
    magnitude = 1.0f / magnitude;
  }

  return *this * magnitude;
}

float Vector3::MagnitudeSquared() const {
  return x() * x() + y() * y() + z() * z();
}

float Vector3::Magnitude() const {
  return sqrt(MagnitudeSquared());
}

float Vector3::Dot(const Vector3& b) const {
  return x() * b.x() + y() * b.y() + z() * b.z();
}

Vector3 Vector3::Cross(const Vector3& b) const {
  return Vector3(c_[1] * b.c_[2] - c_[2] * b.c_[1],
    c_[2] * b.c_[0] - c_[0] * b.c_[2],
    c_[0] * b.c_[1] - c_[1] * b.c_[0]);
}

Vector3 Vector3::Slerp(Vector3 start, Vector3 end, float percent) {
  // Make sure both start and end are normalized.
  start = start.Normalized();
  end = end.Normalized();

  float dot = start.Dot(end);
  dot = FMath::Clamp(dot, -1.0f, 1.0f);
  float theta = FMath::Acos(dot) * percent;
  Vector3 relative_vector = end - start * dot;
  relative_vector = relative_vector.Normalized();
  return ((start * FMath::Cos(theta)) + (relative_vector * FMath::Sin(theta)));
}

Vector3 Vector3::Scale(const Vector3& a, const Vector3& b) {
  Vector3 result = a;
  result.Scale(b);
  return result;
}

float Vector3::AngleDegrees(const Vector3& a, const Vector3& b) {
  float dot = a.Dot(b);
  float angleRadians = acos(dot);
  return Util::ToDegrees(angleRadians);
}

/*
* Definitions for Quaternion class
*/

Quaternion::Quaternion() {
  q_[0] = 1.0f;
  q_[1] = 0.0f;
  q_[2] = 0.0f;
  q_[3] = 0.0f;
}

Quaternion::Quaternion(const float& a, const float& b, const float& c, const float& d) {
  q_[0] = a;
  q_[1] = b;
  q_[2] = c;
  q_[3] = d;
}

float Quaternion::w() const {
  return q_[0];
}

float Quaternion::x() const {
  return q_[1];
}

float Quaternion::y() const {
  return q_[2];
}

float Quaternion::z() const {
  return q_[3];
}

Quaternion& Quaternion::operator+=(const Quaternion& q) {
  q_[0] += q.q_[0];
  q_[1] += q.q_[1];
  q_[2] += q.q_[2];
  q_[3] += q.q_[3];
  return *this;
}
Quaternion Quaternion::operator+(const Quaternion& q) const {
  return Quaternion(*this) += q;
}

Quaternion& Quaternion::operator-=(const Quaternion& q) {
  q_[0] -= q.q_[0];
  q_[1] -= q.q_[1];
  q_[2] -= q.q_[2];
  q_[3] -= q.q_[3];
  return *this;
}
Quaternion Quaternion::operator-(const Quaternion& q) const {
  return Quaternion(*this) -= q;
}

Quaternion& Quaternion::operator*=(const Quaternion& q) {
  float a = q_[0] * q.q_[0] - q_[1] * q.q_[1] - q_[2] * q.q_[2] - q_[3] * q.q_[3];
  float b = q_[0] * q.q_[1] + q_[1] * q.q_[0] + q_[2] * q.q_[3] - q_[3] * q.q_[2];
  float c = q_[0] * q.q_[2] - q_[1] * q.q_[3] + q_[2] * q.q_[0] + q_[3] * q.q_[1];
  float d = q_[0] * q.q_[3] + q_[1] * q.q_[2] - q_[2] * q.q_[1] + q_[3] * q.q_[0];
  q_[0] = a;
  q_[1] = b;
  q_[2] = c;
  q_[3] = d;
  return *this;
}
Quaternion Quaternion::operator*(const Quaternion& q) const {
  return Quaternion(*this) *= q;
}

Quaternion& Quaternion::operator*=(const float& k) {
  q_[0] *= k;
  q_[1] *= k;
  q_[2] *= k;
  q_[3] *= k;
  return *this;
}
Quaternion Quaternion::operator*(const float& k) const {
  return Quaternion(*this) *= k;
}
const float& Quaternion::operator[](int index) const {
  return q_[index];
}
float& Quaternion::operator[](int index) {
  return q_[index];
}

void Quaternion::Set(const float& a, const float& b,
  const float& c, const float& d) {
  q_[0] = a;
  q_[1] = b;
  q_[2] = c;
  q_[3] = d;
}

float Quaternion::Dot(const Quaternion& q) const {
  return q_[0] * q.q_[0] + q_[1] * q.q_[1] + q_[2] * q.q_[2] + q_[3] * q.q_[3];
}

void Quaternion::Rotate(Vector3* p_v) const {
  Vector3& v = *p_v;
  const float& w = q_[0];
  const float& x = q_[1];
  const float& y = q_[2];
  const float& z = q_[3];

  const float kTwo = 2.0f;
  float vcoeff = kTwo * w * w - 1.0f;
  float ucoeff = kTwo * (x * v.x() + y * v.y() + z * v.z());
  float ccoeff = kTwo * w;

  float out0 = vcoeff * v.x() + ucoeff * x + ccoeff * (y * v.z() - z * v.y());
  float out1 = vcoeff * v.y() + ucoeff * y + ccoeff * (z * v.x() - x * v.z());
  float out2 = vcoeff * v.z() + ucoeff * z + ccoeff * (x * v.y() - y * v.x());

  v.Set(out0, out1, out2);
}

Vector3 Quaternion::Rotated(const Vector3& v) const {
  Vector3 result(v);
  Rotate(&result);
  return result;
}

Quaternion Quaternion::Normalized() {
  float n = Norm();
  if (n != 0.0f) {
    return (*this) * (1.0f / n);
  }

  return Quaternion();
}

Quaternion Quaternion::Inverted() const {
  return Quaternion(q_[0], -q_[1], -q_[2], -q_[3]);
}


float Quaternion::Norm() const {
  return sqrt(NormSquared());
}


float Quaternion::NormSquared() const {
  return Dot(*this);
}

Quaternion Quaternion::FromToRotation(const Vector3& from_direction, const Vector3& to_direction) {
  float dot = from_direction.Dot(to_direction);
  float norm = sqrt(from_direction.MagnitudeSquared() * to_direction.MagnitudeSquared());
  float real = norm + dot;
  Vector3 w;

  if (real < 1.e-6f * norm) {
    real = 0.0f;
    w = fabsf(from_direction.x()) > fabsf(from_direction.z()) ?
      Vector3(-from_direction.y(), from_direction.x(), 0.0f)
      : Vector3(0.0f, -from_direction.z(), from_direction.y());
  } else {
    w = from_direction.Cross(to_direction);
  }

  Quaternion result = Quaternion(real, w.x(), w.y(), w.z());

  return result.Normalized();
}

Quaternion Quaternion::AxisAngle(const Vector3& axis, float angle) {
  angle *= PI / 180.0f;

  float factor = sin(angle / 2.0f);

  float x = axis.x() * factor;
  float y = axis.y() * factor;
  float z = axis.z() * factor;
  float w = cos(angle / 2.0f);

  return Quaternion(w, x, y, z);
}

Quaternion Quaternion::Lerp(const Quaternion& a, const Quaternion& b, float t) {
  return (a * (1.0f - t) + b * t).Normalized();
}

float Quaternion::AngleDegrees(const Quaternion& a, const Quaternion& b) {
  return (b * a.Inverted()).w();
}
