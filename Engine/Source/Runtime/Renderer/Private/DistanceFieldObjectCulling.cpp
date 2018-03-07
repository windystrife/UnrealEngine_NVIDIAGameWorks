// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldObjectCulling.cpp
=============================================================================*/

#include "DistanceFieldAmbientOcclusion.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "ScreenRendering.h"
#include "DistanceFieldLightingPost.h"
#include "OneColorShader.h"
#include "GlobalDistanceField.h"
#include "FXSystem.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

int32 GAOScatterTileCulling = 1;
FAutoConsoleVariableRef CVarAOScatterTileCulling(
	TEXT("r.AOScatterTileCulling"),
	GAOScatterTileCulling,
	TEXT("Whether to use the rasterizer for binning occluder objects into screenspace tiles."),
	ECVF_RenderThreadSafe
	);

class FCircleVertexBuffer : public FVertexBuffer
{
public:

	int32 NumSections;

	FCircleVertexBuffer()
	{
		NumSections = 8;
	}

	virtual void InitRHI() override
	{
		// Used as a non-indexed triangle list, so 3 vertices per triangle
		const uint32 Size = 3 * NumSections * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, Buffer);
		FScreenVertex* DestVertex = (FScreenVertex*)Buffer;

		const float RadiansPerRingSegment = PI / (float)NumSections;

		// Boost the effective radius so that the edges of the circle approximation lie on the circle, instead of the vertices
		const float Radius = 1.0f / FMath::Cos(RadiansPerRingSegment);

		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			float Fraction = SectionIndex / (float)NumSections;
			float CurrentAngle = Fraction * 2 * PI;
			float NextAngle = ((SectionIndex + 1) / (float)NumSections) * 2 * PI;
			FVector2D CurrentPosition(Radius * FMath::Cos(CurrentAngle), Radius * FMath::Sin(CurrentAngle));
			FVector2D NextPosition(Radius * FMath::Cos(NextAngle), Radius * FMath::Sin(NextAngle));

			DestVertex[SectionIndex * 3 + 0].Position = FVector2D(0, 0);
			DestVertex[SectionIndex * 3 + 0].UV = CurrentPosition;
			DestVertex[SectionIndex * 3 + 1].Position = FVector2D(0, 0);
			DestVertex[SectionIndex * 3 + 1].UV = NextPosition;
			DestVertex[SectionIndex * 3 + 2].Position = FVector2D(0, 0);
			DestVertex[SectionIndex * 3 + 2].UV = FVector2D(.5f, .5f);
		}

		RHIUnlockVertexBuffer(VertexBufferRHI);      
	}
};

TGlobalResource<FCircleVertexBuffer> GCircleVertexBuffer;
TGlobalResource<FDistanceFieldObjectBufferResource> GAOCulledObjectBuffers;

void FTileIntersectionResources::InitDynamicRHI()
{
	const uint32 FastVRamFlag = GFastVRamConfig.DistanceFieldTileIntersectionResources | (IsTransientResourceBufferAliasingEnabled() ? BUF_Transient : BUF_None);
	TileConeAxisAndCos.Initialize(sizeof(FVector4), TileDimensions.X * TileDimensions.Y, PF_A32B32G32R32F, BUF_Static | FastVRamFlag, TEXT("TileConeAxisAndCos"));
	TileConeDepthRanges.Initialize(sizeof(FVector4), TileDimensions.X * TileDimensions.Y, PF_A32B32G32R32F, BUF_Static | FastVRamFlag, TEXT("TileConeDepthRanges"));

	NumCulledTilesArray.Initialize(sizeof(uint32), MaxSceneObjects, PF_R32_UINT, BUF_Static | FastVRamFlag, TEXT("NumCulledTilesArray"));
	CulledTilesStartOffsetArray.Initialize(sizeof(uint32), MaxSceneObjects, PF_R32_UINT, BUF_Static | FastVRamFlag, TEXT("CulledTilesStartOffsetArray"));

	// Can only use 16 bit for CulledTileDataArray if few enough objects and tiles
	const bool b16BitObjectIndices = MaxSceneObjects < (1 << 16);
	const bool b16BitCulledTileIndexBuffer = bAllow16BitIndices && b16BitObjectIndices && TileDimensions.X * TileDimensions.Y < (1 << 16);
	CulledTileDataArray.Initialize(
		b16BitCulledTileIndexBuffer ? sizeof(uint16) : sizeof(uint32), 
		GMaxDistanceFieldObjectsPerCullTile * TileDimensions.X * TileDimensions.Y * CulledTileDataStride, 
		b16BitCulledTileIndexBuffer ? PF_R16_UINT : PF_R32_UINT, 
		BUF_Static | FastVRamFlag, 
		TEXT("CulledTileDataArray"));
	ObjectTilesIndirectArguments.Initialize(sizeof(uint32), 3, PF_R32_UINT, BUF_Static | BUF_DrawIndirect);
}

class FCullObjectsForViewCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullObjectsForViewCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FCullObjectsForViewCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
		CulledObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		NumConvexHullPlanes.Bind(Initializer.ParameterMap, TEXT("NumConvexHullPlanes"));
		ViewFrustumConvexHull.Bind(Initializer.ParameterMap, TEXT("ViewFrustumConvexHull"));
		ObjectBoundingGeometryIndexCount.Bind(Initializer.ParameterMap, TEXT("ObjectBoundingGeometryIndexCount"));
	}

	FCullObjectsForViewCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		FUnorderedAccessViewRHIParamRef OutUAVs[6];
		OutUAVs[0] = GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GAOCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GAOCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GAOCulledObjectBuffers.Buffers.BoxBounds.UAV;
		OutUAVs[4] = Scene->DistanceFieldSceneData.ObjectBuffers->Data.UAV;
		OutUAVs[5] = Scene->DistanceFieldSceneData.ObjectBuffers->Bounds.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer);
		CulledObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		// Shader assumes max 6
		check(View.ViewFrustum.Planes.Num() <= 6);
		SetShaderValue(RHICmdList, ShaderRHI, NumConvexHullPlanes, View.ViewFrustum.Planes.Num());
		SetShaderValueArray(RHICmdList, ShaderRHI, ViewFrustumConvexHull, View.ViewFrustum.Planes.GetData(), View.ViewFrustum.Planes.Num());
		SetShaderValue(RHICmdList, ShaderRHI, ObjectBoundingGeometryIndexCount, StencilingGeometry::GLowPolyStencilSphereIndexBuffer.GetIndexCount());
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers));
		CulledObjectParameters.UnsetParameters(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[6];
		OutUAVs[0] = GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GAOCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GAOCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GAOCulledObjectBuffers.Buffers.BoxBounds.UAV;
		OutUAVs[4] = Scene->DistanceFieldSceneData.ObjectBuffers->Data.UAV;
		OutUAVs[5] = Scene->DistanceFieldSceneData.ObjectBuffers->Bounds.UAV;		
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectBufferParameters;
		Ar << CulledObjectParameters;
		Ar << AOParameters;
		Ar << NumConvexHullPlanes;
		Ar << ViewFrustumConvexHull;
		Ar << ObjectBoundingGeometryIndexCount;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
	FDistanceFieldCulledObjectBufferParameters CulledObjectParameters;
	FAOParameters AOParameters;
	FShaderParameter NumConvexHullPlanes;
	FShaderParameter ViewFrustumConvexHull;
	FShaderParameter ObjectBoundingGeometryIndexCount;
};

IMPLEMENT_SHADER_TYPE(,FCullObjectsForViewCS,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("CullObjectsForViewCS"),SF_Compute);

void CullObjectsToView(FRHICommandListImmediate& RHICmdList, FScene* Scene, const FViewInfo& View, const FDistanceFieldAOParameters& Parameters, FDistanceFieldObjectBufferResource& CulledObjectBuffers)
{
	SCOPED_DRAW_EVENT(RHICmdList, ObjectFrustumCulling);

	if (!CulledObjectBuffers.IsInitialized()
		|| CulledObjectBuffers.Buffers.MaxObjects < Scene->DistanceFieldSceneData.NumObjectsInBuffer
		|| CulledObjectBuffers.Buffers.MaxObjects > 3 * Scene->DistanceFieldSceneData.NumObjectsInBuffer)
	{
		CulledObjectBuffers.Buffers.MaxObjects = Scene->DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
		CulledObjectBuffers.ReleaseResource();
		CulledObjectBuffers.InitResource();
	}
	CulledObjectBuffers.Buffers.AcquireTransientResource();

	{
		ClearUAV(RHICmdList, CulledObjectBuffers.Buffers.ObjectIndirectArguments, 0);

		TShaderMapRef<FCullObjectsForViewCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		ComputeShader->SetParameters(RHICmdList, Scene, View, Parameters);

		DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, UpdateObjectsGroupSize), 1, 1);
		ComputeShader->UnsetParameters(RHICmdList, Scene);
	}
}

/**  */
class FBuildTileConesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildTileConesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		
		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FBuildTileConesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		TileConeAxisAndCos.Bind(Initializer.ParameterMap, TEXT("TileConeAxisAndCos"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
	}

	FBuildTileConesCS()
	{
	}
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldNormal, FScene* Scene, FVector2D NumGroupsValue, const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = TileIntersectionResources->TileConeAxisAndCos.UAV;
		OutUAVs[1] = TileIntersectionResources->TileConeDepthRanges.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		TileConeAxisAndCos.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileConeAxisAndCos);
		TileConeDepthRanges.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileConeDepthRanges);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		TileConeAxisAndCos.UnsetUAV(RHICmdList, GetComputeShader());
		TileConeDepthRanges.UnsetUAV(RHICmdList, GetComputeShader());

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = TileIntersectionResources->TileConeAxisAndCos.UAV;
		OutUAVs[1] = TileIntersectionResources->TileConeDepthRanges.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << AOParameters;
		Ar << TileConeAxisAndCos;
		Ar << TileConeDepthRanges;
		Ar << NumGroups;
		Ar << ViewDimensionsParameter;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
	FRWShaderParameter TileConeAxisAndCos;
	FRWShaderParameter TileConeDepthRanges;
	FShaderParameter ViewDimensionsParameter;
	FShaderParameter NumGroups;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
};

IMPLEMENT_SHADER_TYPE(,FBuildTileConesCS,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("BuildTileConesMain"),SF_Compute);

/**  */
class FObjectCullVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FObjectCullVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform); 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FObjectCullVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		ConservativeRadiusScale.Bind(Initializer.ParameterMap, TEXT("ConservativeRadiusScale"));
	}

	FObjectCullVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		const int32 NumRings = StencilingGeometry::GLowPolyStencilSphereVertexBuffer.GetNumRings();
		const float RadiansPerRingSegment = PI / (float)NumRings;

		// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
		const float ConservativeRadiusScaleValue = 1.0f / FMath::Cos(RadiansPerRingSegment);

		SetShaderValue(RHICmdList, ShaderRHI, ConservativeRadiusScale, ConservativeRadiusScaleValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << ConservativeRadiusScale;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FShaderParameter ConservativeRadiusScale;
};

IMPLEMENT_SHADER_TYPE(,FObjectCullVS,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("ObjectCullVS"),SF_Vertex);

template <bool bCountingPass>
class TObjectCullPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TObjectCullPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform) && RHISupportsPixelShaderUAVs(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FTileIntersectionParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SCATTER_CULLING_COUNT_PASS"), bCountingPass ? 1 : 0);
	}

	/** Default constructor. */
	TObjectCullPS() {}

	/** Initialization constructor. */
	TObjectCullPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		TileIntersectionParameters.Bind(Initializer.ParameterMap);
		TileConeAxisAndCos.Bind(Initializer.ParameterMap, TEXT("TileConeAxisAndCos"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FVector2D NumGroupsValue, const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		SetSRVParameter(RHICmdList, ShaderRHI, TileConeAxisAndCos, TileIntersectionResources->TileConeAxisAndCos.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);
		TileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void GetUAVs(const FSceneView& View, TArray<FUnorderedAccessViewRHIParamRef>& UAVs)
	{
		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;
		TileIntersectionParameters.GetUAVs(*TileIntersectionResources, UAVs);
		check(UAVs.Num() > 0);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << TileIntersectionParameters;
		Ar << TileConeAxisAndCos;
		Ar << TileConeDepthRanges;
		Ar << NumGroups;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FTileIntersectionParameters TileIntersectionParameters;
	FShaderResourceParameter TileConeAxisAndCos;
	FShaderResourceParameter TileConeDepthRanges;
	FShaderParameter NumGroups;
};

IMPLEMENT_SHADER_TYPE(template<>,TObjectCullPS<true>,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("ObjectCullPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TObjectCullPS<false>,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("ObjectCullPS"),SF_Pixel);

const uint32 ComputeStartOffsetGroupSize = 64;

/**  */
class FComputeCulledTilesStartOffsetCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeCulledTilesStartOffsetCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		FTileIntersectionParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("COMPUTE_START_OFFSET_GROUP_SIZE"), ComputeStartOffsetGroupSize);
	}

	FComputeCulledTilesStartOffsetCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		TileIntersectionParameters.Bind(Initializer.ParameterMap);
	}

	FComputeCulledTilesStartOffsetCS()
	{
	}
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;
		
		TArray<FUnorderedAccessViewRHIParamRef> UAVs;
		TileIntersectionParameters.GetUAVs(*TileIntersectionResources, UAVs);

		RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EComputeToCompute, UAVs.GetData(), UAVs.Num());

		TileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		TileIntersectionParameters.UnsetParameters(RHICmdList, GetComputeShader());

		TArray<FUnorderedAccessViewRHIParamRef> UAVs;
		TileIntersectionParameters.GetUAVs(*TileIntersectionResources, UAVs);

		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, UAVs.GetData(), UAVs.Num());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << TileIntersectionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FTileIntersectionParameters TileIntersectionParameters;
};

IMPLEMENT_SHADER_TYPE(,FComputeCulledTilesStartOffsetCS,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("ComputeCulledTilesStartOffsetCS"),SF_Compute);

template<bool bCountingPass>
void ScatterTilesToObjects(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FIntPoint TileListGroupSize, const FDistanceFieldAOParameters& Parameters)
{
	TShaderMapRef<FObjectCullVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TObjectCullPS<bCountingPass>> PixelShader(View.ShaderMap);

	TArray<FUnorderedAccessViewRHIParamRef> UAVs;
	PixelShader->GetUAVs(View, UAVs);
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, UAVs.GetData(), UAVs.Num());
	if (GRHIRequiresRenderTargetForPixelShaderUAVs)
	{
		TRefCountPtr<IPooledRenderTarget> Dummy;
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(TileListGroupSize, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Dummy, TEXT("Dummy"));
		FRHIRenderTargetView DummyRTView(Dummy->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ENoAction);
		RHICmdList.SetRenderTargets(1, &DummyRTView, NULL, UAVs.Num(), UAVs.GetData());
	}
	else
	{
		RHICmdList.SetRenderTargets(0, (const FRHIRenderTargetView*)NULL, NULL, UAVs.Num(), UAVs.GetData());
	}
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	RHICmdList.SetViewport(0, 0, 0.0f, TileListGroupSize.X, TileListGroupSize.Y, 1.0f);

	// Render backfaces since camera may intersect
	GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(RHICmdList, View, Parameters);
	PixelShader->SetParameters(RHICmdList, View, FVector2D(TileListGroupSize.X, TileListGroupSize.Y), Parameters);

	RHICmdList.SetStreamSource(0, StencilingGeometry::GLowPolyStencilSphereVertexBuffer.VertexBufferRHI, 0);

	RHICmdList.DrawIndexedPrimitiveIndirect(
		PT_TriangleList,
		StencilingGeometry::GLowPolyStencilSphereIndexBuffer.IndexBufferRHI, 
		GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.Buffer,
		0);
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, UAVs.GetData(), UAVs.Num());

	SetRenderTarget(RHICmdList, NULL, NULL);
}

FIntPoint BuildTileObjectLists(FRHICommandListImmediate& RHICmdList, FScene* Scene, TArray<FViewInfo>& Views, FSceneRenderTargetItem& DistanceFieldNormal, const FDistanceFieldAOParameters& Parameters)
{
	SCOPED_DRAW_EVENT(RHICmdList, BuildTileList);
	SetRenderTarget(RHICmdList, NULL, NULL);

	FIntPoint TileListGroupSize;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		TileListGroupSize = FIntPoint(
			FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX), 
			FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY));

		FTileIntersectionResources*& TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		if (!TileIntersectionResources 
			|| !TileIntersectionResources->IsInitialized() 
			|| !TileIntersectionResources->HasAllocatedEnoughFor(TileListGroupSize, Scene->DistanceFieldSceneData.NumObjectsInBuffer)
			|| GFastVRamConfig.bDirty )
		{
			if (TileIntersectionResources)
			{
				TileIntersectionResources->ReleaseResource();
			}
			else
			{
				TileIntersectionResources = new FTileIntersectionResources(!IsMetalPlatform(GShaderPlatformForFeatureLevel[View.FeatureLevel]));
			}
			
			TileIntersectionResources->SetupParameters(TileListGroupSize, Scene->DistanceFieldSceneData.NumObjectsInBuffer);
			TileIntersectionResources->InitResource();
		}
		TileIntersectionResources->AcquireTransientResource();

		if (GAOScatterTileCulling)
		{
			{
				SCOPED_DRAW_EVENT(RHICmdList, BuildTileCones);
				TShaderMapRef<FBuildTileConesCS> ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, Scene, FVector2D(TileListGroupSize.X, TileListGroupSize.Y), Parameters);
				DispatchComputeShader(RHICmdList, *ComputeShader, TileListGroupSize.X, TileListGroupSize.Y, 1);

				ComputeShader->UnsetParameters(RHICmdList, View);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, CountTileObjectIntersections);

				// Start at 0 tiles per object
				ClearUAV(RHICmdList, TileIntersectionResources->NumCulledTilesArray, 0);

				// Rasterize object bounding shapes and intersect with screen tiles to compute how many tiles intersect each object
				ScatterTilesToObjects<true>(RHICmdList, View, TileListGroupSize, Parameters);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, ComputeStartOffsets);
				// Start at 0 threadgroups
				ClearUAV(RHICmdList, TileIntersectionResources->ObjectTilesIndirectArguments, 0);

				// Accumulate how many cone trace threadgroups we should dispatch, and also compute the start offset for each object's culled tile data
				TShaderMapRef<FComputeCulledTilesStartOffsetCS> ComputeShader(View.ShaderMap);
				const uint32 GroupSize = FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, ComputeStartOffsetGroupSize);
				// Must write to RWObjectTilesIndirectArguments
				check(GroupSize != 0);
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View);
				DispatchComputeShader(RHICmdList, *ComputeShader, GroupSize, 1, 1);

				ComputeShader->UnsetParameters(RHICmdList, View);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, CullTilesToObjects);

				// Start at 0 tiles per object
				ClearUAV(RHICmdList, TileIntersectionResources->NumCulledTilesArray, 0);

				// Rasterize object bounding shapes and intersect with screen tiles, and write out intersecting tile indices for the cone tracing pass
				ScatterTilesToObjects<false>(RHICmdList, View, TileListGroupSize, Parameters);
			}
		}
		else
		{
			ensure(0);
		}
	}

	return TileListGroupSize;
}
