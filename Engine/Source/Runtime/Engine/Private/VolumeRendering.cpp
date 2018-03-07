// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumeRendering.cpp: Volume rendering implementation.
=============================================================================*/

#include "VolumeRendering.h"
#include "ScreenRendering.h"
#include "RHIStaticStates.h"

IMPLEMENT_SHADER_TYPE(,FWriteToSliceGS,TEXT("/Engine/Private/TranslucentLightingShaders.usf"),TEXT("WriteToSliceMainGS"),SF_Geometry);
IMPLEMENT_SHADER_TYPE(,FWriteToSliceVS,TEXT("/Engine/Private/TranslucentLightingShaders.usf"),TEXT("WriteToSliceMainVS"),SF_Vertex);

TGlobalResource<FVolumeRasterizeVertexBuffer> GVolumeRasterizeVertexBuffer;

/** Draws a quad per volume texture slice to the subregion of the volume texture specified. */
ENGINE_API void RasterizeToVolumeTexture(FRHICommandList& RHICmdList, FVolumeBounds VolumeBounds)
{
	// Setup the viewport to only render to the given bounds
	RHICmdList.SetViewport(VolumeBounds.MinX, VolumeBounds.MinY, 0, VolumeBounds.MaxX, VolumeBounds.MaxY, 0);
	RHICmdList.SetStreamSource(0, GVolumeRasterizeVertexBuffer.VertexBufferRHI, 0);
	const int32 NumInstances = VolumeBounds.MaxZ - VolumeBounds.MinZ;
	// Render a quad per slice affected by the given bounds
	RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, NumInstances);
}
