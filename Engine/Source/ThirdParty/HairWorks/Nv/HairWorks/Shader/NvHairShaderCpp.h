/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_HW_SHADER_CPP_H
#define NV_HW_SHADER_CPP_H

// Code that allows the hlsl header to be included in cpp code
#ifndef _CPP
#	error "Can only be included in C++ code"
#endif

typedef NvCo_Vec4 float4;
typedef NvCo_Vec3 float3;
typedef NvCo_Vec3 float3;
typedef NvCo_EleRowMat4x4 float4x4;

#ifndef row_major
#define row_major		
#endif


#ifndef NOINTERPOLATION
#	define	NOINTERPOLATION					
#endif

#endif // NV_HW_SHADER_CPP_H
