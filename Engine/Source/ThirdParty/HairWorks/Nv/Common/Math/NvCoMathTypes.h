/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_MATH_TYPES_H
#define NV_CO_MATH_TYPES_H

#include <Nv/Core/1.0/NvTypes.h>

/** \addtogroup common
@{
*/

// Vec types are unaligned. 
struct NvCo_Vec2
{
	float x, y;
};

struct NvCo_Vec3
{
	float x, y, z;
};

struct NvCo_Vec4
{
	float x, y, z, w;
};

struct NvCo_EleRowMat4x4
{
	float _11, _12, _13, _14;
	float _21, _22, _23, _24;
	float _31, _32, _33, _34;
	float _41, _42, _43, _44;
};

struct NvCo_VecRowMat4x4
{
	NvCo_Vec4 rows[4];
};

/// Quaternion
struct NvCo_Quaternion
{
	float x, y, z, w;
};

struct NvCo_DualQuaternion
{
	// Interpolation is from q0 to q1
	NvCo_Quaternion q0, q1;
};

struct NvCo_Transform3
{
	NvCo_Quaternion q;
	NvCo_Vec3 p;
};

struct NvCo_Bounds3
{
	NvCo_Vec3 minimum;
	NvCo_Vec3 maximum;
};

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Aligned versions !!!!!!!!!!!!!!!!!!!!!!! */

/// AlignedVec types are aligned for faster Simd type operations

/// 2D point or vector
NV_ALIGN(8, struct NvCo_AlignedVec2)
{
	float x, y;
};

/// 4D point or vector
NV_ALIGN(16, struct NvCo_AlignedVec4)
{
	float x, y, z, w;
};

/// 3 matrix, implicit row major
/// NOTE that the rows are Vec4 to allow easy map to
struct NvCo_AlignedRowMat3
{
	NvCo_AlignedVec4 rows[3];
};

/// 4x4 matrix, implicit row major
struct NvCo_AlignedRowMat4
{
	NvCo_AlignedVec4 rows[4];
};

NV_ALIGN(16, struct NvCo_AlignedElementMat4)
{
	float _11, _12, _13, _14;
	float _21, _22, _23, _24;
	float _31, _32, _33, _34;
	float _41, _42, _43, _44;
};

/// Quaternion
NV_ALIGN(16, struct NvCo_AlignedQuaternion)
{
	float x, y, z, w;
};

/// Rigid body transformation
struct NvCo_AlignedTransform3
{
	NvCo_AlignedQuaternion q;
	NvCo_AlignedVec4 p;
};

/// Axis-aligned bounding box
struct NvCo_AlignedBounds3
{
	NvCo_AlignedVec4 minimum;
	NvCo_AlignedVec4 maximum;
};

struct NvCo_AlignedDualQuaternion
{
	// Interpolation is from q0 to q1
	NvCo_AlignedQuaternion q0, q1;
};


#ifdef __cplusplus

namespace nvidia  {
namespace Common {

typedef NvCo_Vec2 Vec2;
typedef NvCo_Vec3 Vec3;
typedef NvCo_Vec4 Vec4;

typedef NvCo_EleRowMat4x4 EleRowMat4x4;
typedef NvCo_VecRowMat4x4 VecRowMat4x4;

/// Quaternion
typedef NvCo_Quaternion Quaternion;
typedef NvCo_DualQuaternion DualQuaternion;
typedef NvCo_Transform3 Transform3;

typedef NvCo_Bounds3 Bounds3;

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Aligned versions !!!!!!!!!!!!!!!!!!!!!!! */

typedef NvCo_AlignedVec2 AlignedVec2;
typedef NvCo_AlignedVec4 AlignedVec4;
typedef NvCo_AlignedRowMat3 AlignedRowMat3;
typedef NvCo_AlignedRowMat4 AlignedRowMat4;

typedef NvCo_AlignedElementMat4 AlignedElementMat4;
typedef NvCo_AlignedQuaternion AlignedQuaternion;
typedef NvCo_AlignedTransform3 AlignedTransform3;
typedef NvCo_AlignedBounds3 AlignedBounds3;
typedef NvCo_AlignedDualQuaternion AlignedDualQuaternion;

/* Some simple methods to set up */

NV_FORCE_INLINE Void setUnitX(Vec4& in) { in.x = 1.0f; in.y = 0.0f; in.z = 0.0f; in.w = 0.0f; }
NV_FORCE_INLINE Void setUnitY(Vec4& in) { in.x = 0.0f; in.y = 1.0f; in.z = 0.0f; in.w = 0.0f; }
NV_FORCE_INLINE Void setUnitZ(Vec4& in) { in.x = 0.0f; in.y = 0.0f; in.z = 1.0f; in.w = 0.0f; }
NV_FORCE_INLINE Void setUnitW(Vec4& in) { in.x = 0.0f; in.y = 0.0f; in.z = 0.0f; in.w = 1.0f; }

NV_FORCE_INLINE Void setAll(Vec4& in, Float32 v) { in.x = in.y = in.z = in.w = v; }
NV_FORCE_INLINE Void setAll(Vec3& in, Float32 v) { in.x = in.y = in.z = v; }

NV_FORCE_INLINE Void setZero(Vec4& in) { in.x = in.y = in.z = in.w = 0.0f; }
NV_FORCE_INLINE Void setZero(Vec3& in) { in.x = in.y = in.z = 0.0f; }

NV_FORCE_INLINE Void setIdentity(EleRowMat4x4& in) 
{ 
	in._11 = 1.0f; in._12 = 0.0f; in._13 = 0.0f; in._14 = 0.0f; 
	in._21 = 0.0f; in._22 = 1.0f; in._23 = 0.0f; in._24 = 0.0f; 
	in._31 = 0.0f; in._32 = 0.0f; in._33 = 1.0f; in._34 = 0.0f; 
	in._41 = 0.0f; in._42 = 0.0f; in._43 = 0.0f; in._44 = 1.0f;
}
NV_FORCE_INLINE Void setIdentity(VecRowMat4x4& in) { setIdentity((EleRowMat4x4&)in); }

} // namespace Common 
} // namespace nvidia

#endif // __cplusplus

/** @} */

#endif // NV_CO_MATH_TYPES_H
