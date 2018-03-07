/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */
#ifndef NV_CO_GFSDK_MATH_H
#define NV_CO_GFSDK_MATH_H

#include <Nv/Core/1.0/NvDefines.h>

// Note! This library is based on the legacy gfsdk maths library!
// It should probably NOT be used for new projects, and is provided for compatibility.

#include "float.h"

#include <Nv/Common/Math/NvCoMathTypes.h>
#include <Nv/Common/Math/NvCoMath.h>

namespace nvidia {
namespace Common {
namespace GfsdkMath { 
/*! \namespace nvidia::Common::GfsdkMath
\brief Legacy maths library based on gfsdk_ maths library.
\details Maths library for doing graphics related maths. Uses NvCoMathTypes.h types as underlying vector and matrix types. 
*/

// implicit row major
typedef NvCo_EleRowMat4x4 Mat4x4;

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Vec2 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

inline Vec2 makeVec2(float x, float y)
{
	Vec2 v;
	v.x = x;
	v.y = y;
	return v;
}

inline Vec2& operator*=(Vec2& v, float s)
{
	v.x *= s;
	v.y *= s;
	return v;
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Vec3 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

inline Vec3 makeVec3(float x, float y, float z)
{
	Vec3 v = { x, y, z };
	return v;
}

inline Vec3 operator+(const Vec3& p0, const Vec3& p1) 
{
	return makeVec3(p0.x + p1.x, p0.y + p1.y, p0.z + p1.z);
}

inline Vec3& operator+=(Vec3& v, const Vec3& v1)
{
	v.x += v1.x; v.y += v1.y; v.z += v1.z;
	return v;
}

inline Vec3 operator-(const Vec3& p0, const Vec3& p1) 
{
	return makeVec3(p0.x - p1.x, p0.y - p1.y, p0.z - p1.z);
}

inline Vec3& operator-=(Vec3& v, const Vec3& v1)
{
	v.x -= v1.x; v.y -= v1.y; v.z -= v1.z;
	return v;
}

inline Vec3 operator*(float s, const Vec3& p) 
{
	return makeVec3(s * p.x, s * p.y, s * p.z);
}

inline Vec3 operator*(const Vec3& p, float s) 
{
	return makeVec3(s * p.x, s * p.y, s * p.z);
}

inline float dot(const Vec3& v0, const Vec3& v1) 
{
	return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
}

inline float calcLengthSquared(const Vec3& v) 
{
	return dot(v,v);
}

inline float calcLength(const Vec3& v) 
{
	return Math::sqrt(calcLengthSquared(v));
}

inline Vec3 calcNormalized(const Vec3& v) 
{
	float l = calcLength(v);
	if (l != 0.0f)
	{
		return v * (1.0f / l);
	}
	return v;
}

inline Vec3 cross(const Vec3& v1, const Vec3& v2)
{
	return makeVec3(
		v1.y * v2.z - v1.z * v2.y, 
		v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x);
}

inline Vec3 lerp(const Vec3& v1, const Vec3& v2, float t)
{
	return makeVec3(Math::lerp(v1.x, v2.x, t), Math::lerp(v1.y, v2.y, t), Math::lerp(v1.z, v2.z, t));
}

inline Vec3 calcMin(const Vec3& v1, const Vec3& v2)
{
	return makeVec3(
		Math::calcMin(v1.x, v2.x),
		Math::calcMin(v1.y, v2.y),
		Math::calcMin(v1.z, v2.z)
		);
}

inline Vec3 calcMax(const Vec3& v1, const Vec3& v2)
{
	return makeVec3(
		Math::calcMax(v1.x, v2.x),
		Math::calcMax(v1.y, v2.y),
		Math::calcMax(v1.z, v2.z)
		);
}

inline float calcHorizontalMin(const Vec3& v)
{
	return Math::calcMin(Math::calcMin(v.x, v.y), v.z);
}

inline float calcHorizontalMax(const Vec3& v)
{
	return Math::calcMax(Math::calcMax(v.x, v.y), v.z);
}

inline bool	equal(const Vec3& a, const Vec3& b)
{
	return (&a == &b) || (a.x == b.x && a.y == b.y && a.z == b.z);
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Vec4 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

inline Vec4 makeVec4(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 0.0f)
{
	Vec4 v = { x, y, z, w };
	return v;
}

inline Vec4 makeVec4(const Vec3& v, float w = 0.0f)
{
	Vec4 nv = {v.x, v.y, v.z, w};
	return nv;
}

inline bool	equal(const Vec4& a, const Vec4& b)
{
	return (&a == &b) || (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w);
}

inline Vec4 operator+(const Vec4& p0, const Vec4& p1) 
{
	return makeVec4(p0.x + p1.x, p0.y + p1.y, p0.z + p1.z, p0.w + p1.w);
}

inline Vec4& operator+=(Vec4& v, const Vec4& v1)
{
	v.x += v1.x; v.y += v1.y; v.z += v1.z; v.w += v1.w;
	return v;
}

inline Vec4 operator-(const Vec4& p0, const Vec4& p1) 
{
	return makeVec4(p0.x - p1.x, p0.y - p1.y, p0.z - p1.z, p0.w - p1.w);
}

inline Vec4& operator-=(Vec4 &v, const Vec4& v1)
{
	v.x -= v1.x; v.y -= v1.y; v.z -= v1.z; v.w -= v1.w;
	return v;
}

inline Vec4 operator*(float s, const Vec4& p) 
{
	return makeVec4(s * p.x, s * p.y, s * p.z, s * p.w);
}

inline Vec4 operator*(const Vec4& p, float s) 
{
	return makeVec4(s * p.x, s * p.y, s * p.z, s * p.w);
}

inline float dot(const Vec4& v0, const Vec4& v1) 
{
	return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z + v0.w * v1.w;
}

inline float calcLengthSquared(const Vec4& v) 
{
	return dot(v,v);
}

inline float calcLength(const Vec4& v) 
{
	return Math::sqrt(calcLengthSquared(v));
}

inline float calcLength3(const Vec4& v) 
{
	return Math::sqrt(calcLengthSquared((Vec3&)v));
}

inline const Vec4 calcNormalized(const Vec4& v) 
{
	Vec4 nv = v;

	float l = calcLength(nv);
	if (l > FLT_EPSILON)
	{
		const float s = 1.0f / l;

		nv.x *= s; 
		nv.y *= s; 
		nv.z *= s; 
		nv.w *= s;
	}

	return nv;
}

inline Vec4 lerp(const Vec4& v1, const Vec4& v2, float t)
{
	return makeVec4(
		Math::lerp(v1.x, v2.x, t),
		Math::lerp(v1.y, v2.y, t),
		Math::lerp(v1.z, v2.z, t),
		Math::lerp(v1.w, v2.w, t)
		);
}

inline Quaternion	asQuaternion(const Vec4& in)
{
	union { Quaternion q; Vec4 v; } u;
	u.v = in;
	return u.q;
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Quaternion !!!!!!!!!!!!!!!!!!!!!!!!!!! */

inline Quaternion makeQuaternion(float x, float y, float z, float w)
{
	Quaternion q = {x, y, z, w };
	return q;
}

Quaternion	getConjugate(const Quaternion& in);
void				setIdentity(Quaternion& q);
Quaternion	quaternionMultiply(const Quaternion& q0, const Quaternion& q1);
Vec3		rotate(const Quaternion& q, const Vec3& v);
Vec3		rotateInv(const Quaternion& q, const Vec3& v);
Quaternion	slerp(const Quaternion& q0, const Quaternion& q1, float t);
Vec3		getBasisX(const Quaternion& q);
Vec3		getBasisY(const Quaternion& q);
Vec3		getBasisZ(const Quaternion& q);
void				setRotation(const Vec3& xaxis, const Vec3& yaxis, const Vec3& zaxis, Quaternion& m);
Quaternion	makeRotation(const Vec3& axis, float radian);
Quaternion	rotateBetween(const Vec3& from, const Vec3& to);
Quaternion	quatFromAxis(const Vec3& n, const Vec3& t);

inline Vec4	asVec4(const Quaternion& q)
{
	union { Quaternion q; Vec4 v; } u;
	u.q = q;
	return u.v;
}

inline const Quaternion calcNormalized(const Quaternion &q) 
{
	Quaternion nq = q;
	float l = calcLength(asVec4(nq));
	if (l > FLT_EPSILON)
	{
		const float s = 1.0f / l;

		nq.x *= s;
		nq.y *= s;
		nq.z *= s;
		nq.w *= s;
	}

	return nq;
}

inline Quaternion operator+(const Quaternion& p0, const Quaternion& p1)
{
	return makeQuaternion(p0.x + p1.x, p0.y + p1.y, p0.z + p1.z, p0.w + p1.w);
}

inline Quaternion& operator+=(Quaternion& v, const Quaternion& v1)
{
	v.x += v1.x; v.y += v1.y; v.z += v1.z; v.w += v1.w;
	return v;
}

inline Quaternion operator-(const Quaternion& p0, const Quaternion& p1)
{
	return makeQuaternion(p0.x - p1.x, p0.y - p1.y, p0.z - p1.z, p0.w - p1.w);
}

inline Quaternion& operator-=(Quaternion& v, const Quaternion& v1)
{
	v.x -= v1.x; v.y -= v1.y; v.z -= v1.z; v.w -= v1.w;
	return v;
}

inline Quaternion operator*(float s, const Quaternion& q)
{
	Quaternion nq = { s * q.x, s * q.y, s * q.z, s * q.w };
	return nq;
}

inline Quaternion operator*(const Quaternion& q, float s) 
{
	Quaternion nq = { s * q.x, s * q.y, s * q.z, s * q.w };
	return nq;
}

inline float dot(const Quaternion& q0, const Quaternion& q1) 
{
	return q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! Mat4x4 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

void				setIdentity(Mat4x4& out);
void				setDiagonal(const Vec3& d, Mat4x4& out);
void				setMat4x4(const float* p, Mat4x4& out);
void				setScale(const Vec3& s, Mat4x4& out);
void				setRotation(const Vec3& xaxis, const Vec3& yaxis, const Vec3& zaxis, Mat4x4& mOut);
void				setRotation(const Quaternion& q, Mat4x4& out);
void				setRotation(const Vec3& from, const Vec3& to, Mat4x4& m);
void				setTranslation(const Vec3& t, Mat4x4& out);

Mat4x4		makeTransform(const Quaternion& q, const Vec3& t, const Vec3& s);

bool				equal(const Mat4x4& a, const Mat4x4& b);

Vec3		transformCoord(const Mat4x4& m, Vec3 op);
Vec3		transformVector(const Mat4x4& m, Vec3 op);

float				getDeterminant(const Mat4x4& m);
Mat4x4		inverse(const Mat4x4& m);
void				orthonormalize(Mat4x4& m);

Mat4x4		operator*(const Mat4x4& in1, const Mat4x4& in2);
Mat4x4&		operator*=(Mat4x4& m, float s);
Mat4x4&		operator+=(Mat4x4& m1, const Mat4x4& m2);

Quaternion	calcRotation(const Mat4x4& m);
Vec3		getScale(const Mat4x4& m);
Vec3		getTranslation(const Mat4x4& m);
void				setTranslation(Mat4x4& m, const Vec3& v);

NV_FORCE_INLINE float&			getElement(Mat4x4& m, int row, int col) { return (&m._11)[row * 4 + col]; }
NV_FORCE_INLINE const float&	getElement(const Mat4x4& m, int row, int col) { return (&m._11)[row * 4 + col]; }

Vec3		getColumn(const Mat4x4& m, int col);
Vec3		getRow(const Mat4x4& m, int row);
void				setColumn(const Mat4x4& m, int col, const Vec3& v);
void				setRow(const Mat4x4& m, int row, const Vec3& v);

Mat4x4		lerp(Mat4x4& start, Mat4x4& end, float t);
bool				inverseProjection(Mat4x4& out, const Mat4x4& in);
void				calcPolarDecomposition(const Mat4x4& a, Mat4x4& r);

bool				isLeftHanded(const Mat4x4& m);
Mat4x4		makeOrthoLh(float orthoW, float orthoH, float zNear, float zFar);
Mat4x4		makeOrthoRh(float orthoW, float orthoH, float zNear, float zFar);

/* !!!!!!!!!!!!!!!!!!!!!!!!!!! DualQuaternion !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */


DualQuaternion	makeDualQuaternion(const Quaternion& q, const Vec3& t);
DualQuaternion	makeDualQuaternion(const Mat4x4& m);
Mat4x4			makeTransform(const DualQuaternion& dq);
void			setIdentity(DualQuaternion& dq);
void			setZero(DualQuaternion& dq);

DualQuaternion	calcNormalized(const DualQuaternion& dq);

DualQuaternion	operator*(float s, const DualQuaternion& dq);
DualQuaternion	operator*(const DualQuaternion& dq, float s);
DualQuaternion&	operator+=(DualQuaternion& dq, const DualQuaternion& dq2);
DualQuaternion	lerp(const DualQuaternion& dq1, const DualQuaternion& dq2, float t);
Vec3			transformCoord(const DualQuaternion& dq, const Vec3& vecIn);
Vec3			transformVector(const DualQuaternion& dq, const Vec3& vecIn);
Quaternion		calcRotation(const DualQuaternion& dq);
Vec3			getTranslation(const DualQuaternion& dq);


} // namespace GfsdkMath
} // namespace Common  
} // namespace nvidia 

namespace NvCoGfsdkMath = nvidia::Common::GfsdkMath;

#endif 
