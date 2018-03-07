// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldAmbientOcclusion.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ScenePrivate.h"

const static int32 GAOMaxSupportedLevel = 6;
/** Number of cone traced directions. */
const int32 NumConeSampleDirections = 9;

/** Base downsample factor that all distance field AO operations are done at. */
const int32 GAODownsampleFactor = 2;

extern const uint32 UpdateObjectsGroupSize;

inline bool DoesPlatformSupportDistanceFieldAO(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5 || Platform == SP_PS4 || Platform == SP_XBOXONE_D3D12 || (IsMetalPlatform(Platform) && GetMaxSupportedFeatureLevel(Platform) >= ERHIFeatureLevel::SM5 && RHIGetShaderLanguageVersion(Platform) >= 2) || Platform == SP_VULKAN_SM5;
}

extern FIntPoint GetBufferSizeForAO();

class FDistanceFieldAOParameters
{
public:
	float GlobalMaxOcclusionDistance;
	float ObjectMaxOcclusionDistance;
	float Contrast;

	FDistanceFieldAOParameters(float InOcclusionMaxDistance, float InContrast = 0);
};

/**  */
class FTileIntersectionResources : public FRenderResource
{
public:

	FRWBuffer TileConeAxisAndCos;
	FRWBuffer TileConeDepthRanges;

	FRWBuffer NumCulledTilesArray;
	FRWBuffer CulledTilesStartOffsetArray;
	FRWBuffer CulledTileDataArray;
	FRWBuffer ObjectTilesIndirectArguments;

	FIntPoint TileDimensions;
	int32 MaxSceneObjects;
	bool bAllow16BitIndices;

	FTileIntersectionResources(bool bInAllow16BitIndices) :
		MaxSceneObjects(0), bAllow16BitIndices(bInAllow16BitIndices)
	{}

	bool HasAllocatedEnoughFor(FIntPoint TestTileDimensions, int32 TestMaxSceneObjects) const
	{
		return TestTileDimensions == TileDimensions && TestMaxSceneObjects <= MaxSceneObjects;
	}

	void SetupParameters(FIntPoint InTileDimensions, int32 InMaxSceneObjects)
	{
		TileDimensions = InTileDimensions;
		MaxSceneObjects = InMaxSceneObjects;
	}

	virtual void InitDynamicRHI() override;

	

	virtual void ReleaseDynamicRHI() override
	{
		TileConeAxisAndCos.Release();
		TileConeDepthRanges.Release();

		NumCulledTilesArray.Release();
		CulledTilesStartOffsetArray.Release();
		CulledTileDataArray.Release();
		ObjectTilesIndirectArguments.Release();
	}

	void AcquireTransientResource()
	{
		TileConeAxisAndCos.AcquireTransientResource();
		TileConeDepthRanges.AcquireTransientResource();
		NumCulledTilesArray.AcquireTransientResource();
		CulledTilesStartOffsetArray.AcquireTransientResource();
		CulledTileDataArray.AcquireTransientResource();
	}

	void DiscardTransientResource()
	{
		TileConeAxisAndCos.DiscardTransientResource();
		TileConeDepthRanges.DiscardTransientResource();
		NumCulledTilesArray.DiscardTransientResource();
		CulledTilesStartOffsetArray.DiscardTransientResource();
		CulledTileDataArray.DiscardTransientResource();
	}

	size_t GetSizeBytes() const
	{
		return TileConeAxisAndCos.NumBytes + TileConeDepthRanges.NumBytes
			+ NumCulledTilesArray.NumBytes + CulledTilesStartOffsetArray.NumBytes + CulledTileDataArray.NumBytes + ObjectTilesIndirectArguments.NumBytes;
	}
};

static int32 CulledTileDataStride = 2;
static int32 ConeTraceObjectsThreadGroupSize = 64;

class FTileIntersectionParameters
{
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("CULLED_TILE_DATA_STRIDE"), CulledTileDataStride);
		extern int32 GDistanceFieldAOTileSizeX;
		OutEnvironment.SetDefine(TEXT("CULLED_TILE_SIZEX"), GDistanceFieldAOTileSizeX);
		extern int32 GConeTraceDownsampleFactor;
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);
		OutEnvironment.SetDefine(TEXT("CONE_TRACE_OBJECTS_THREADGROUP_SIZE"), ConeTraceObjectsThreadGroupSize);
	}

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		TileListGroupSize.Bind(ParameterMap, TEXT("TileListGroupSize"));
		NumCulledTilesArray.Bind(ParameterMap, TEXT("NumCulledTilesArray"));
		CulledTilesStartOffsetArray.Bind(ParameterMap, TEXT("CulledTilesStartOffsetArray"));
		CulledTileDataArray.Bind(ParameterMap, TEXT("CulledTileDataArray"));
		ObjectTilesIndirectArguments.Bind(ParameterMap, TEXT("ObjectTilesIndirectArguments"));
	}

	template<typename TParamRef>
	void Set(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FTileIntersectionResources& TileIntersectionResources)
	{
		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileIntersectionResources.TileDimensions);

		NumCulledTilesArray.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources.NumCulledTilesArray);
		CulledTilesStartOffsetArray.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources.CulledTilesStartOffsetArray);
		CulledTileDataArray.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources.CulledTileDataArray);
		ObjectTilesIndirectArguments.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources.ObjectTilesIndirectArguments);
	}

	void GetUAVs(FTileIntersectionResources& TileIntersectionResources, TArray<FUnorderedAccessViewRHIParamRef>& UAVs)
	{
		uint32 MaxIndex = 0;
		
		MaxIndex = FMath::Max(MaxIndex, NumCulledTilesArray.GetUAVIndex());
		MaxIndex = FMath::Max(MaxIndex, CulledTilesStartOffsetArray.GetUAVIndex());
		MaxIndex = FMath::Max(MaxIndex, CulledTileDataArray.GetUAVIndex());
		MaxIndex = FMath::Max(MaxIndex, ObjectTilesIndirectArguments.GetUAVIndex());

		UAVs.AddZeroed(MaxIndex + 1);

		if (NumCulledTilesArray.IsUAVBound())
		{
			UAVs[NumCulledTilesArray.GetUAVIndex()] = TileIntersectionResources.NumCulledTilesArray.UAV;
		}

		if (CulledTilesStartOffsetArray.IsUAVBound())
		{
			UAVs[CulledTilesStartOffsetArray.GetUAVIndex()] = TileIntersectionResources.CulledTilesStartOffsetArray.UAV;
		}

		if (CulledTileDataArray.IsUAVBound())
		{
			UAVs[CulledTileDataArray.GetUAVIndex()] = TileIntersectionResources.CulledTileDataArray.UAV;
		}

		if (ObjectTilesIndirectArguments.IsUAVBound())
		{
			UAVs[ObjectTilesIndirectArguments.GetUAVIndex()] = TileIntersectionResources.ObjectTilesIndirectArguments.UAV;
		}

		check(UAVs.Num() > 0);
	}

	template<typename TParamRef>
	void UnsetParameters(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI)
	{
		NumCulledTilesArray.UnsetUAV(RHICmdList, ShaderRHI);
		CulledTilesStartOffsetArray.UnsetUAV(RHICmdList, ShaderRHI);
		CulledTileDataArray.UnsetUAV(RHICmdList, ShaderRHI);
		ObjectTilesIndirectArguments.UnsetUAV(RHICmdList, ShaderRHI);
	}

	friend FArchive& operator<<(FArchive& Ar, FTileIntersectionParameters& P)
	{
		Ar << P.TileListGroupSize;
		Ar << P.NumCulledTilesArray;
		Ar << P.CulledTilesStartOffsetArray;
		Ar << P.CulledTileDataArray;
		Ar << P.ObjectTilesIndirectArguments;
		return Ar;
	}

private:
	FShaderParameter TileListGroupSize;
	
	FRWShaderParameter NumCulledTilesArray;
	FRWShaderParameter CulledTilesStartOffsetArray;
	FRWShaderParameter CulledTileDataArray;
	FRWShaderParameter ObjectTilesIndirectArguments;
};

class FAOScreenGridResources : public FRenderResource
{
public:

	FAOScreenGridResources() :
		bAllocateResourceForGI(false)
	{}

	virtual void InitDynamicRHI() override;

	virtual void ReleaseDynamicRHI() override
	{
		ScreenGridConeVisibility.Release();
		ConeDepthVisibilityFunction.Release();
		StepBentNormal.Release();
		SurfelIrradiance.Release();
		HeightfieldIrradiance.Release();
	}

	void AcquireTransientResource()
	{
		ScreenGridConeVisibility.AcquireTransientResource();
		if (bAllocateResourceForGI)
		{
			StepBentNormal.AcquireTransientResource();
			SurfelIrradiance.AcquireTransientResource();
			HeightfieldIrradiance.AcquireTransientResource();
		}
	}

	void DiscardTransientResource()
	{
		ScreenGridConeVisibility.DiscardTransientResource();
		if (bAllocateResourceForGI)
		{
			StepBentNormal.DiscardTransientResource();
			SurfelIrradiance.DiscardTransientResource();
			HeightfieldIrradiance.DiscardTransientResource();
		}
	}

	FIntPoint ScreenGridDimensions;

	FRWBuffer ScreenGridConeVisibility;

	bool bAllocateResourceForGI;
	FRWBuffer ConeDepthVisibilityFunction;
	FRWBuffer StepBentNormal;
	FRWBuffer SurfelIrradiance;
	FRWBuffer HeightfieldIrradiance;

	size_t GetSizeBytesForAO() const
	{
		return ScreenGridConeVisibility.NumBytes;
	}

	size_t GetSizeBytesForGI() const
	{
		return ConeDepthVisibilityFunction.NumBytes + StepBentNormal.NumBytes + SurfelIrradiance.NumBytes + HeightfieldIrradiance.NumBytes;
	}
};

extern void GetSpacedVectors(uint32 FrameNumber, TArray<FVector, TInlineAllocator<9> >& OutVectors);

BEGIN_UNIFORM_BUFFER_STRUCT(FAOSampleData2,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,SampleDirections,[NumConeSampleDirections])
END_UNIFORM_BUFFER_STRUCT(FAOSampleData2)

class FAOParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		AOObjectMaxDistance.Bind(ParameterMap,TEXT("AOObjectMaxDistance"));
		AOStepScale.Bind(ParameterMap,TEXT("AOStepScale"));
		AOStepExponentScale.Bind(ParameterMap,TEXT("AOStepExponentScale"));
		AOMaxViewDistance.Bind(ParameterMap,TEXT("AOMaxViewDistance"));
		AOGlobalMaxOcclusionDistance.Bind(ParameterMap,TEXT("AOGlobalMaxOcclusionDistance"));
	}

	friend FArchive& operator<<(FArchive& Ar,FAOParameters& Parameters)
	{
		Ar << Parameters.AOObjectMaxDistance;
		Ar << Parameters.AOStepScale;
		Ar << Parameters.AOStepExponentScale;
		Ar << Parameters.AOMaxViewDistance;
		Ar << Parameters.AOGlobalMaxOcclusionDistance;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FDistanceFieldAOParameters& Parameters)
	{
		SetShaderValue(RHICmdList, ShaderRHI, AOObjectMaxDistance, Parameters.ObjectMaxOcclusionDistance);

		extern float GAOConeHalfAngle;
		const float AOLargestSampleOffset = Parameters.ObjectMaxOcclusionDistance / (1 + FMath::Tan(GAOConeHalfAngle));

		extern float GAOStepExponentScale;
		extern uint32 GAONumConeSteps;
		float AOStepScaleValue = AOLargestSampleOffset / FMath::Pow(2.0f, GAOStepExponentScale * (GAONumConeSteps - 1));
		SetShaderValue(RHICmdList, ShaderRHI, AOStepScale, AOStepScaleValue);

		SetShaderValue(RHICmdList, ShaderRHI, AOStepExponentScale, GAOStepExponentScale);

		extern float GetMaxAOViewDistance();
		SetShaderValue(RHICmdList, ShaderRHI, AOMaxViewDistance, GetMaxAOViewDistance());

		const float GlobalMaxOcclusionDistance = Parameters.GlobalMaxOcclusionDistance;
		SetShaderValue(RHICmdList, ShaderRHI, AOGlobalMaxOcclusionDistance, GlobalMaxOcclusionDistance);
	}

private:
	FShaderParameter AOObjectMaxDistance;
	FShaderParameter AOStepScale;
	FShaderParameter AOStepExponentScale;
	FShaderParameter AOMaxViewDistance;
	FShaderParameter AOGlobalMaxOcclusionDistance;
};

inline float GetMaxAOViewDistance()
{
	extern float GAOMaxViewDistance;
	// Scene depth stored in fp16 alpha, must fade out before it runs out of range
	// The fade extends past GAOMaxViewDistance a bit
	return FMath::Min(GAOMaxViewDistance, 65000.0f);
}

class FMaxSizedRWBuffers : public FRenderResource
{
public:
	FMaxSizedRWBuffers()
	{
		MaxSize = 0;
	}

	virtual void InitDynamicRHI()
	{
		check(0);
	}

	virtual void ReleaseDynamicRHI()
	{
		check(0);
	}

	void AllocateFor(int32 InMaxSize)
	{
		bool bReallocate = false;

		if (InMaxSize > MaxSize)
		{
			MaxSize = InMaxSize;
			bReallocate = true;
		}

		if (!IsInitialized())
		{
			InitResource();
		}
		else if (bReallocate)
		{
			UpdateRHI();
		}
	}

	int32 GetMaxSize() const { return MaxSize; }

protected:
	int32 MaxSize;
};

// Must match usf
const int32 RecordConeDataStride = 10;
// In float4s, must match usf
const int32 NumVisibilitySteps = 10;

/**  */
class FTemporaryIrradianceCacheResources : public FMaxSizedRWBuffers
{
public:

	virtual void InitDynamicRHI()
	{
		if (MaxSize > 0)
		{
			ConeVisibility.Initialize(sizeof(float), MaxSize * NumConeSampleDirections, PF_R32_FLOAT, BUF_Static);
			ConeData.Initialize(sizeof(float), MaxSize * NumConeSampleDirections * RecordConeDataStride, PF_R32_FLOAT, BUF_Static);
			StepBentNormal.Initialize(sizeof(float) * 4, MaxSize * NumVisibilitySteps, PF_A32B32G32R32F, BUF_Static);
			SurfelIrradiance.Initialize(sizeof(FFloat16Color), MaxSize, PF_FloatRGBA, BUF_Static);
			HeightfieldIrradiance.Initialize(sizeof(FFloat16Color), MaxSize, PF_FloatRGBA, BUF_Static);
		}
	}

	virtual void ReleaseDynamicRHI()
	{
		ConeVisibility.Release();
		ConeData.Release();
		StepBentNormal.Release();
		SurfelIrradiance.Release();
		HeightfieldIrradiance.Release();
	}

	size_t GetSizeBytes() const
	{
		return ConeVisibility.NumBytes + ConeData.NumBytes + StepBentNormal.NumBytes + SurfelIrradiance.NumBytes + HeightfieldIrradiance.NumBytes;
	}

	FRWBuffer ConeVisibility;
	FRWBuffer ConeData;
	FRWBuffer StepBentNormal;
	FRWBuffer SurfelIrradiance;
	FRWBuffer HeightfieldIrradiance;
};

class FScreenGridParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		BaseLevelTexelSize.Bind(ParameterMap, TEXT("BaseLevelTexelSize"));
		JitterOffset.Bind(ParameterMap, TEXT("JitterOffset"));
		ScreenGridConeVisibilitySize.Bind(ParameterMap, TEXT("ScreenGridConeVisibilitySize"));
		DistanceFieldNormalTexture.Bind(ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(ParameterMap, TEXT("DistanceFieldNormalSampler"));
	}

	template<typename TParamRef>
	void Set(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FViewInfo& View, FSceneRenderTargetItem& DistanceFieldNormal)
	{
		const FIntPoint DownsampledBufferSize = GetBufferSizeForAO();
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BaseLevelTexelSize, BaseLevelTexelSizeValue);

		extern FVector2D GetJitterOffset(int32 SampleIndex);
		SetShaderValue(RHICmdList, ShaderRHI, JitterOffset, GetJitterOffset(View.ViewState->GetDistanceFieldTemporalSampleIndex()));

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;

		SetShaderValue(RHICmdList, ShaderRHI, ScreenGridConeVisibilitySize, ScreenGridResources->ScreenGridDimensions);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);
	}

	friend FArchive& operator<<(FArchive& Ar,FScreenGridParameters& P)
	{
		Ar << P.BaseLevelTexelSize << P.JitterOffset << P.ScreenGridConeVisibilitySize << P.DistanceFieldNormalTexture << P.DistanceFieldNormalSampler;
		return Ar;
	}

private:
	FShaderParameter BaseLevelTexelSize;
	FShaderParameter JitterOffset;
	FShaderParameter ScreenGridConeVisibilitySize;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
};

extern void TrackGPUProgress(FRHICommandListImmediate& RHICmdList, uint32 DebugId);

extern bool ShouldRenderDeferredDynamicSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily);

extern void CullObjectsToView(FRHICommandListImmediate& RHICmdList, FScene* Scene, const FViewInfo& View, const FDistanceFieldAOParameters& Parameters, FDistanceFieldObjectBufferResource& CulledObjectBuffers);
extern FIntPoint BuildTileObjectLists(FRHICommandListImmediate& RHICmdList, FScene* Scene, TArray<FViewInfo>& Views, FSceneRenderTargetItem& DistanceFieldNormal, const FDistanceFieldAOParameters& Parameters);
