/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_HAIR_COMMON_H
#define NV_HAIR_COMMON_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/Math/NvCoMathTypes.h>
#include <Nv/Common/Math/NvCoMath.h>

/** \addtogroup HairWorks
@{
*/

// Set up the default namespaces
namespace nvidia {
namespace HairWorks {

// Make Math functions available in this namespace 
typedef NvCo::Math Math;

// Make the NvCommon maths types the hair sdk math types 
typedef NvCo_Vec2 Vec2;
typedef NvCo_Vec3 Vec3;
typedef NvCo_Vec4 Vec4;
typedef NvCo_Quaternion Quaternion;
typedef NvCo_DualQuaternion DualQuaternion;
// Row major 4x4 matrix
typedef NvCo_EleRowMat4x4 Mat4x4;

} // namespace HairWorks
namespace Hair = HairWorks;
} // namespace nvidia
namespace NvHair = nvidia::Hair;

/** @} */

#endif // NV_HAIR_COMMON_H