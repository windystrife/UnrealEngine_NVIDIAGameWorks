// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumeRendering.h: Volume rendering definitions.
=============================================================================*/

#pragma once

#include "Shader.h"
#include "ShaderParameters.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "ScreenRendering.h"

/** Represents a subregion of a volume texture. */
struct FVolumeBounds
{
	int32 MinX, MinY, MinZ;
	int32 MaxX, MaxY, MaxZ;

	FVolumeBounds() :
		MinX(0),
		MinY(0),
		MinZ(0),
		MaxX(0),
		MaxY(0),
		MaxZ(0)
	{}

	FVolumeBounds(int32 Max) :
		MinX(0),
		MinY(0),
		MinZ(0),
		MaxX(Max),
		MaxY(Max),
		MaxZ(Max)
	{}

	bool IsValid() const
	{
		return MaxX > MinX && MaxY > MinY && MaxZ > MinZ;
	}
};

/** Vertex shader used to write to a range of slices of a 3d volume texture. */
class FWriteToSliceVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FWriteToSliceVS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4); 
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.CompilerFlags.Add( CFLAG_VertexToGeometryShader );
	}

	FWriteToSliceVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		UVScaleBias.Bind(Initializer.ParameterMap, TEXT("UVScaleBias"));
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
	}

	FWriteToSliceVS() {}

	template <typename TRHICommandList>
	void SetParameters(TRHICommandList& RHICmdList, const FVolumeBounds& VolumeBounds, FIntVector VolumeResolution)
	{
		const float InvVolumeResolutionX = 1.0f / VolumeResolution.X;
		const float InvVolumeResolutionY = 1.0f / VolumeResolution.Y;
		SetShaderValue(RHICmdList, GetVertexShader(), UVScaleBias, FVector4(
			(VolumeBounds.MaxX - VolumeBounds.MinX) * InvVolumeResolutionX,
			(VolumeBounds.MaxY - VolumeBounds.MinY) * InvVolumeResolutionY,
			VolumeBounds.MinX * InvVolumeResolutionX,
			VolumeBounds.MinY * InvVolumeResolutionY));
		SetShaderValue(RHICmdList, GetVertexShader(), MinZ, VolumeBounds.MinZ);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << UVScaleBias;
		Ar << MinZ;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter UVScaleBias;
	FShaderParameter MinZ;
};

/** Geometry shader used to write to a range of slices of a 3d volume texture. */
class FWriteToSliceGS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FWriteToSliceGS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && RHISupportsGeometryShaders(Platform); 
	}

	FWriteToSliceGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
	}
	FWriteToSliceGS() {}

	template <typename TRHICommandList>
	void SetParameters(TRHICommandList& RHICmdList, int32 MinZValue)
	{
		SetShaderValue(RHICmdList, GetGeometryShader(), MinZ, MinZValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MinZ;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter MinZ;
};

extern ENGINE_API void RasterizeToVolumeTexture(FRHICommandList& RHICmdList, FVolumeBounds VolumeBounds);

/** Vertex buffer used for rendering into a volume texture. */
class FVolumeRasterizeVertexBuffer : public FVertexBuffer
{
public:

	virtual void InitRHI() override
	{
		// Used as a non-indexed triangle strip, so 4 vertices per quad
		const uint32 Size = 4 * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, Buffer);		
		FScreenVertex* DestVertex = (FScreenVertex*)Buffer;

		// Setup a full - render target quad
		// A viewport and UVScaleBias will be used to implement rendering to a sub region
		DestVertex[0].Position = FVector2D(1, -GProjectionSignY);
		DestVertex[0].UV = FVector2D(1, 1);
		DestVertex[1].Position = FVector2D(1, GProjectionSignY);
		DestVertex[1].UV = FVector2D(1, 0);
		DestVertex[2].Position = FVector2D(-1, -GProjectionSignY);
		DestVertex[2].UV = FVector2D(0, 1);
		DestVertex[3].Position = FVector2D(-1, GProjectionSignY);
		DestVertex[3].UV = FVector2D(0, 0);

		RHIUnlockVertexBuffer(VertexBufferRHI);      
	}
};

extern ENGINE_API TGlobalResource<FVolumeRasterizeVertexBuffer> GVolumeRasterizeVertexBuffer;