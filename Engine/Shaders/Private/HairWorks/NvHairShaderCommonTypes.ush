/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_HAIR_SHADER_COMMON_TYPES_H
#define NV_HAIR_SHADER_COMMON_TYPES_H

#ifdef _CPP 
// This include is needed to make the HLSL types interpretable in C++
#	include "NvHairShaderCpp.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////
struct NvHair_ShaderAttributes
{
	float3	P;			// world coord position
	float3	T;			// world space tangent vector
	float3	N;			// world space normal vector at the root
	float4	texcoords; // texture coordinates on hair root 
						// .xy: texcoord on the hair root
						// .z: texcoord along the hair
						// .w: texcoord along the hair quad
	float3	V;			// world space view vector
	float	hairID;		// unique hair identifier

#if defined(NV_HAIR_DECLARE_VELOCITY_ATTR)
	float3	wVel;		// 'pixel velocity'. In view space, difference current pixel position, and previous pixel position. W is W after view projection. Look at NvHair_WorldToScreen for details.
#endif
};

//////////////////////////////////////////////////////////////////////////////
// basic hair material from constant buffer
//////////////////////////////////////////////////////////////////////////////
// 9 float4
struct NvHair_Material 
{
	// 3 float4
	float4			rootColor; 
	float4			tipColor; 
	float4			specularColor; 

	// 4 floats (= 1 float4)
	float			diffuseBlend;
	float			diffuseScale;
	float			diffuseHairNormalWeight;
	float			_diffuseUnused_; // for alignment and future use

	// 4 floats (= 1 float4)
	float			specularPrimaryScale;
	float			specularPrimaryPower;
	float			specularPrimaryBreakup;
	float			specularNoiseScale;

	// 4 floats (= 1 float4)
	float			specularSecondaryScale;
	float			specularSecondaryPower;
	float			specularSecondaryOffset;
	float			_specularUnused_; // for alignment and future use

	// 4 floats (= 1 float4)
	float			rootTipColorWeight;
	float			rootTipColorFalloff;
	float			shadowSigma;
	float			strandBlendScale;

	// 4 floats (= 1 float4)
	float			glintStrength;
	float			glintCount;
	float			glintExponent;
	float			rootAlphaFalloff;
};

//////////////////////////////////////////////////////////////////////////////
// Use this data structure to manage all hair related cbuffer data within your own cbuffer
struct NvHair_ConstantBuffer
{
	static const int NOISE_TABLE_SIZE = 64;

	// camera information 
	row_major	float4x4	inverseViewProjection; // inverse of view projection matrix
	row_major	float4x4	inverseProjection; // inverse of projection matrix
	row_major	float4x4	inverseViewport; // inverse of viewport transform
	row_major	float4x4	inverseViewProjectionViewport; // inverse of world to screen matrix

	row_major	float4x4	viewProjection; // view projection matrix
	row_major	float4x4	viewport; // viewport matrix

	row_major	float4x4	prevViewProjection; // previous view projection matrix for pixel velocity computation
	row_major	float4x4	prevViewport; // previous viewport matrix for pixel velocity computation

	float4			camPosition;		  // position of camera center
	float4			modelCenter; // center of the growth mesh model

	// shared settings 
	int				useRootColorTexture;
	int				useTipColorTexture; 
	int				useStrandTexture;
	int				useSpecularTexture;

	int				receiveShadows;		
	int				shadowUseLeftHanded;
	float			__shadowReserved1__;
	float			__shadowReserved2__;

	int				strandBlendMode;
	int				colorizeMode;	
	int				strandPointCount;
	int				__reserved__;

	float			lodDistanceFactor;		
	float			lodDetailFactor;		
	float			lodAlphaFactor;
	float			__reservedLOD___;


	NvHair_Material	defaultMaterial; 

	// noise table
	float4			noiseTable[NOISE_TABLE_SIZE]; // 256 floats
};

#endif // NV_HAIR_SHADER_COMMON_TYPES_H
