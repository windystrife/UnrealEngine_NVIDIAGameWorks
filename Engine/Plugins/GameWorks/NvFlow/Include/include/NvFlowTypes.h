/*
 * Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

//! \cond HIDDEN_SYMBOLS
#if defined(_WIN32)
#define NV_FLOW_API extern "C" __declspec(dllexport)
#else
#define NV_FLOW_API
#endif

#define NV_FLOW_API_INLINE inline

//! \endcond

enum NvFlowResult
{
	eNvFlowSuccess = 0,
	eNvFlowFail = 1
};

typedef int NvFlowInt;
typedef unsigned int NvFlowUint;
typedef unsigned long long NvFlowUint64;

struct NvFlowDim
{
	NvFlowUint x, y, z;
};

struct NvFlowUint2
{
	NvFlowUint x, y;
};

struct NvFlowUint3
{
	NvFlowUint x, y, z;
};

struct NvFlowUint4
{
	NvFlowUint x, y, z, w;
};

struct NvFlowInt2
{
	int x, y;
};

struct NvFlowInt3
{
	int x, y, z;
};

struct NvFlowInt4
{
	int x, y, z, w;
};

struct NvFlowFloat2
{
	float x, y;
};

struct NvFlowFloat3
{
	float x, y, z;
};

struct NvFlowFloat4
{
	float x, y, z, w;
};

struct NvFlowFloat4x4
{
	NvFlowFloat4 x, y, z, w;
};

enum NvFlowFormat
{
	eNvFlowFormat_unknown = 0,

	eNvFlowFormat_r32_float = 1,
	eNvFlowFormat_r32g32_float = 2,
	eNvFlowFormat_r32g32b32a32_float = 3,

	eNvFlowFormat_r16_float = 4,
	eNvFlowFormat_r16g16_float = 5,
	eNvFlowFormat_r16g16b16a16_float = 6,

	eNvFlowFormat_r32_uint = 7,
	eNvFlowFormat_r32g32_uint = 8,
	eNvFlowFormat_r32g32b32a32_uint = 9,

	eNvFlowFormat_r8_unorm = 10,
	eNvFlowFormat_r8g8_unorm = 11,
	eNvFlowFormat_r8g8b8a8_unorm = 12,

	eNvFlowFormat_r16_unorm = 13,
	eNvFlowFormat_r16g16_unorm = 14,
	eNvFlowFormat_r16g16b16a16_unorm = 15,

	eNvFlowFormat_d32_float = 16,
	eNvFlowFormat_d24_unorm_s8_uint = 17,

	eNvFlowFormat_r8_snorm = 18,
	eNvFlowFormat_r8g8_snorm = 19,
	eNvFlowFormat_r8g8b8a8_snorm = 20,

	eNvFlowFormat_r32_typeless = 21,
	eNvFlowFormat_r24_unorm_x8_typeless = 22,
	eNvFlowFormat_r24g8_typeless = 23,

	eNvFlowFormat_r16_typeless = 24,
	eNvFlowFormat_d16_unorm = 25,

	eNvFlowFormat_max
};