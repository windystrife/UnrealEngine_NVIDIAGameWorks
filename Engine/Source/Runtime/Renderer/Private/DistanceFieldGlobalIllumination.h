// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldGlobalIllumination.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "DistanceFieldAmbientOcclusion.h"

// In float4's, must match usf
const int32 VPLDataStride = 3;

// Must match usf
const int32 VPLClusterSizeOneDim = 4;

/**  */
class FVPLResources : public FMaxSizedRWBuffers
{
public:

	virtual void InitDynamicRHI()
	{
		if (MaxSize > 0)
		{
			VPLParameterBuffer.Initialize(sizeof(uint32), 2, PF_R32_UINT, BUF_Static);
			VPLDispatchIndirectBuffer.Initialize(sizeof(uint32), 3, PF_R32_UINT, BUF_Static | BUF_DrawIndirect);
			VPLClusterData.Initialize(sizeof(FVector4), MaxSize * VPLDataStride / (VPLClusterSizeOneDim * VPLClusterSizeOneDim), PF_A32B32G32R32F, BUF_Static);
			VPLData.Initialize(sizeof(FVector4), MaxSize * VPLDataStride, PF_A32B32G32R32F, BUF_Static);
		}
	}

	virtual void ReleaseDynamicRHI()
	{
		VPLParameterBuffer.Release();
		VPLDispatchIndirectBuffer.Release();
		VPLClusterData.Release();
		VPLData.Release();
	}

	FRWBuffer VPLParameterBuffer;
	FRWBuffer VPLDispatchIndirectBuffer;
	FRWBuffer VPLClusterData;
	FRWBuffer VPLData;
};
