// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "RenderUtils.h"

// Actual values are used in the shader so do not change
enum EDecalRenderStage
{
	// for DBuffer decals (get proper baked lighting)
	DRS_BeforeBasePass = 0,
	// for volumetrics to update the depth buffer
	DRS_AfterBasePass = 1,
	// for normal decals not modifying the depth buffer
	DRS_BeforeLighting = 2,
	// for rendering decals on mobile
	DRS_Mobile = 3,

	// later we could add "after lighting" and multiply
};

/**
 * Shared decal functionality for deferred and forward shading
 */
struct FDecalRenderingCommon
{
	enum ERenderTargetMode
	{
		RTM_Unknown = -1,
		RTM_SceneColorAndGBufferWithNormal,
		RTM_SceneColorAndGBufferNoNormal,
		RTM_SceneColorAndGBufferDepthWriteWithNormal,
		RTM_SceneColorAndGBufferDepthWriteNoNormal,
		RTM_DBuffer, 
		RTM_GBufferNormal,
		RTM_SceneColor,
	};
	
	static EDecalBlendMode ComputeFinalDecalBlendMode(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode, bool bUseNormal)
	{
		if (!bUseNormal)
		{
			if(DecalBlendMode == DBM_DBuffer_ColorNormalRoughness)
			{
				DecalBlendMode = DBM_DBuffer_ColorRoughness;
			}
			else if(DecalBlendMode == DBM_DBuffer_NormalRoughness)
			{
				DecalBlendMode = DBM_DBuffer_Roughness;
			}
		}
		
		return DecalBlendMode;
	}

	static ERenderTargetMode ComputeRenderTargetMode(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode, bool bHasNormal)
	{
		if (IsMobilePlatform(Platform))
		{
			return RTM_SceneColor;
		}
	
		// Can't modify GBuffers when forward shading, just modify scene color
		if (IsAnyForwardShadingEnabled(Platform)
			&& (DecalBlendMode == DBM_Translucent
				|| DecalBlendMode == DBM_Stain
				|| DecalBlendMode == DBM_Normal))
		{
			return RTM_SceneColor;
		}

		switch(DecalBlendMode)
		{
			case DBM_Translucent:
			case DBM_Stain:
				return bHasNormal ? RTM_SceneColorAndGBufferWithNormal : RTM_SceneColorAndGBufferNoNormal;

			case DBM_Normal:
				return RTM_GBufferNormal;

			case DBM_Emissive:
				return RTM_SceneColor;

			case DBM_DBuffer_ColorNormalRoughness:
			case DBM_DBuffer_Color:
			case DBM_DBuffer_ColorNormal:
			case DBM_DBuffer_ColorRoughness:
			case DBM_DBuffer_Normal:
			case DBM_DBuffer_NormalRoughness:
			case DBM_DBuffer_Roughness:
				// can be optimized using less MRT when possible
				return RTM_DBuffer;

			case DBM_Volumetric_DistanceFunction:
				return bHasNormal ? RTM_SceneColorAndGBufferDepthWriteWithNormal : RTM_SceneColorAndGBufferDepthWriteNoNormal;
		}

		// add the missing decal blend mode to the switch
		check(0);
		return RTM_Unknown;
	}
		
	static EDecalRenderStage ComputeRenderStage(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode)
	{
		if (IsMobilePlatform(Platform))
		{
			return DRS_Mobile;
		}
		
		switch(DecalBlendMode)
		{
			case DBM_DBuffer_ColorNormalRoughness:
			case DBM_DBuffer_Color:
			case DBM_DBuffer_ColorNormal:
			case DBM_DBuffer_ColorRoughness:
			case DBM_DBuffer_Normal:
			case DBM_DBuffer_NormalRoughness:
			case DBM_DBuffer_Roughness:
				return DRS_BeforeBasePass;

			case DBM_Translucent:
			case DBM_Stain:
			case DBM_Normal:
			case DBM_Emissive:
				return DRS_BeforeLighting;
		
			case DBM_Volumetric_DistanceFunction:
				return DRS_AfterBasePass;

			default:
				check(0);
		}
	
		return DRS_BeforeBasePass;
	}

	// @return DECAL_RENDERTARGET_COUNT for the shader
	static uint32 ComputeRenderTargetCount(EShaderPlatform Platform, ERenderTargetMode RenderTargetMode)
	{
		// has to be SceneColor on mobile 
		check(!IsMobilePlatform(Platform) || RenderTargetMode == RTM_SceneColor);

		switch(RenderTargetMode)
		{
			case RTM_SceneColorAndGBufferWithNormal:				return 4;
			case RTM_SceneColorAndGBufferNoNormal:					return 4;
			case RTM_SceneColorAndGBufferDepthWriteWithNormal:		return 5;
			case RTM_SceneColorAndGBufferDepthWriteNoNormal:		return 5;
			case RTM_DBuffer:										return 3;
			case RTM_GBufferNormal:									return 1;
			case RTM_SceneColor:									return 1;
		}

		return 0;
	}
};
