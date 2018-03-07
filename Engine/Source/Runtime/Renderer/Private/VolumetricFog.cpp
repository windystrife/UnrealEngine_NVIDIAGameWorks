// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricFog.cpp
=============================================================================*/

#include "VolumetricFog.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "GlobalDistanceField.h"
#include "GlobalDistanceFieldParameters.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "DistanceFieldLightingShared.h"
#include "VolumetricFogShared.h"
#include "VolumeRendering.h"
#include "ScreenRendering.h"
#include "VolumeLighting.h"
#include "PipelineStateCache.h"

int32 GVolumetricFog = 1;
FAutoConsoleVariableRef CVarVolumetricFog(
	TEXT("r.VolumetricFog"),
	GVolumetricFog,
	TEXT("Whether to allow the volumetric fog feature."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogInjectShadowedLightsSeparately = 1;
FAutoConsoleVariableRef CVarVolumetricFogInjectShadowedLightsSeparately(
	TEXT("r.VolumetricFog.InjectShadowedLightsSeparately"),
	GVolumetricFogInjectShadowedLightsSeparately,
	TEXT("Whether to allow the volumetric fog feature."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GVolumetricFogDepthDistributionScale = 32.0f;
FAutoConsoleVariableRef CVarVolumetricFogDepthDistributionScale(
	TEXT("r.VolumetricFog.DepthDistributionScale"),
	GVolumetricFogDepthDistributionScale,
	TEXT("Scales the slice depth distribution."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogGridPixelSize = 16;
FAutoConsoleVariableRef CVarVolumetricFogGridPixelSize(
	TEXT("r.VolumetricFog.GridPixelSize"),
	GVolumetricFogGridPixelSize,
	TEXT("XY Size of a cell in the voxel grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogGridSizeZ = 64;
FAutoConsoleVariableRef CVarVolumetricFogGridSizeZ(
	TEXT("r.VolumetricFog.GridSizeZ"),
	GVolumetricFogGridSizeZ,
	TEXT("How many Volumetric Fog cells to use in z."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogTemporalReprojection = 1;
FAutoConsoleVariableRef CVarVolumetricFogTemporalReprojection(
	TEXT("r.VolumetricFog.TemporalReprojection"),
	GVolumetricFogTemporalReprojection,
	TEXT("Whether to use temporal reprojection on volumetric fog."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogJitter = 1;
FAutoConsoleVariableRef CVarVolumetricFogJitter(
	TEXT("r.VolumetricFog.Jitter"),
	GVolumetricFogJitter,
	TEXT("Whether to apply jitter to each frame's volumetric fog computation, achieving temporal super sampling."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GVolumetricFogHistoryWeight = .9f;
FAutoConsoleVariableRef CVarVolumetricFogHistoryWeight(
	TEXT("r.VolumetricFog.HistoryWeight"),
	GVolumetricFogHistoryWeight,
	TEXT("How much the history value should be weighted each frame.  This is a tradeoff between visible jittering and responsiveness."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogHistoryMissSupersampleCount = 4;
FAutoConsoleVariableRef CVarVolumetricFogHistoryMissSupersampleCount(
	TEXT("r.VolumetricFog.HistoryMissSupersampleCount"),
	GVolumetricFogHistoryMissSupersampleCount,
	TEXT("Number of lighting samples to compute for voxels whose history value is not available.\n")
	TEXT("This reduces noise when panning or on camera cuts, but introduces a variable cost to volumetric fog computation.  Valid range [1, 16]."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GInverseSquaredLightDistanceBiasScale = 1.0f;
FAutoConsoleVariableRef CVarInverseSquaredLightDistanceBiasScale(
	TEXT("r.VolumetricFog.InverseSquaredLightDistanceBiasScale"),
	GInverseSquaredLightDistanceBiasScale,
	TEXT("Scales the amount added to the inverse squared falloff denominator.  This effectively removes the spike from inverse squared falloff that causes extreme aliasing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FVolumetricFogGlobalData,TEXT("VolumetricFog"));

FVolumetricFogGlobalData::FVolumetricFogGlobalData()
{}

float TemporalHalton( int32 Index, int32 Base )
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while( Index > 0 )
	{
		Result += ( Index % Base ) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

FVector VolumetricFogTemporalRandom(uint32 FrameNumber)
{
	// Center of the voxel
	FVector RandomOffsetValue(.5f, .5f, .5f);

	if (GVolumetricFogJitter && GVolumetricFogTemporalReprojection)
	{
		RandomOffsetValue = FVector(TemporalHalton(FrameNumber & 1023, 2), TemporalHalton(FrameNumber & 1023, 3), TemporalHalton(FrameNumber & 1023, 5));
	}

	return RandomOffsetValue;
}

uint32 VolumetricFogGridInjectionGroupSize = 4;

class FVolumetricFogMaterialSetupCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVolumetricFogMaterialSetupCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogGridInjectionGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FVolumetricFogMaterialSetupCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
		GlobalAlbedo.Bind(Initializer.ParameterMap, TEXT("GlobalAlbedo"));
		GlobalEmissive.Bind(Initializer.ParameterMap, TEXT("GlobalEmissive"));
		GlobalExtinctionScale.Bind(Initializer.ParameterMap, TEXT("GlobalExtinctionScale"));
	} 

	FVolumetricFogMaterialSetupCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FVolumetricFogIntegrationParameterData& IntegrationData,
		const FExponentialHeightFogSceneInfo& FogInfo)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);
		HeightFogParameters.Set(RHICmdList, ShaderRHI, &View);
		SetShaderValue(RHICmdList, ShaderRHI, GlobalAlbedo, FogInfo.VolumetricFogAlbedo);
		SetShaderValue(RHICmdList, ShaderRHI, GlobalEmissive, FogInfo.VolumetricFogEmissive);
		SetShaderValue(RHICmdList, ShaderRHI, GlobalExtinctionScale, FogInfo.VolumetricFogExtinctionScale);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		IPooledRenderTarget* VBufferA,
		IPooledRenderTarget* VBufferB)
	{
		VolumetricFogParameters.UnsetParameters(RHICmdList, GetComputeShader(), View, VBufferA, VBufferB, NULL, false);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumetricFogParameters;
		Ar << HeightFogParameters;
		Ar << GlobalAlbedo;
		Ar << GlobalEmissive;
		Ar << GlobalExtinctionScale;
		return bShaderHasOutdatedParameters;
	}

private:

	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FExponentialHeightFogShaderParameters HeightFogParameters;
	FShaderParameter GlobalAlbedo;
	FShaderParameter GlobalEmissive;
	FShaderParameter GlobalExtinctionScale;
};

IMPLEMENT_SHADER_TYPE(,FVolumetricFogMaterialSetupCS,TEXT("/Engine/Private/VolumetricFog.usf"),TEXT("MaterialSetupCS"),SF_Compute);

/** Vertex shader used to write to a range of slices of a 3d volume texture. */
class FWriteToBoundingSphereVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWriteToBoundingSphereVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_VertexToGeometryShader);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FWriteToBoundingSphereVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
		ViewSpaceBoundingSphere.Bind(Initializer.ParameterMap, TEXT("ViewSpaceBoundingSphere"));
		ViewToVolumeClip.Bind(Initializer.ParameterMap, TEXT("ViewToVolumeClip"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
	}

	FWriteToBoundingSphereVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FVolumetricFogIntegrationParameterData& IntegrationData, const FSphere& BoundingSphere, int32 MinZValue)
	{
		SetShaderValue(RHICmdList, GetVertexShader(), MinZ, MinZValue);

		const FVector ViewSpaceBoundingSphereCenter = View.ViewMatrices.GetViewMatrix().TransformPosition(BoundingSphere.Center);
		SetShaderValue(RHICmdList, GetVertexShader(), ViewSpaceBoundingSphere, FVector4(ViewSpaceBoundingSphereCenter, BoundingSphere.W));

		const FMatrix ProjectionMatrix = View.ViewMatrices.ComputeProjectionNoAAMatrix();
		SetShaderValue(RHICmdList, GetVertexShader(), ViewToVolumeClip, ProjectionMatrix);

		VolumetricFogParameters.Set(RHICmdList, GetVertexShader(), View, IntegrationData);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MinZ;
		Ar << ViewSpaceBoundingSphere;
		Ar << ViewToVolumeClip;
		Ar << VolumetricFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter MinZ;
	FShaderParameter ViewSpaceBoundingSphere;
	FShaderParameter ViewToVolumeClip;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
};

IMPLEMENT_SHADER_TYPE(,FWriteToBoundingSphereVS,TEXT("/Engine/Private/VolumetricFog.usf"),TEXT("WriteToBoundingSphereVS"),SF_Vertex);

/** Shader that adds direct lighting contribution from the given light to the current volume lighting cascade. */
template<bool bDynamicallyShadowed, bool bInverseSquared, bool bTemporalReprojection>
class TInjectShadowedLocalLightPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TInjectShadowedLocalLightPS,Global);
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DYNAMICALLY_SHADOWED"), (uint32)bDynamicallyShadowed);
		OutEnvironment.SetDefine(TEXT("INVERSE_SQUARED_FALLOFF"), (uint32)bInverseSquared);
		OutEnvironment.SetDefine(TEXT("USE_TEMPORAL_REPROJECTION"), bTemporalReprojection);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	TInjectShadowedLocalLightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		PhaseG.Bind(Initializer.ParameterMap, TEXT("PhaseG"));
		InverseSquaredLightDistanceBiasScale.Bind(Initializer.ParameterMap, TEXT("InverseSquaredLightDistanceBiasScale"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		VolumeShadowingParameters.Bind(Initializer.ParameterMap);
	}

	TInjectShadowedLocalLightPS() {}

	// @param InnerSplitIndex which CSM shadow map level, INDEX_NONE if no directional light
	// @param VolumeCascadeIndexValue which volume we render to
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FVolumetricFogIntegrationParameterData& IntegrationData,
		const FLightSceneInfo* LightSceneInfo,
		const FExponentialHeightFogSceneInfo& FogInfo,
		const FProjectedShadowInfo* ShadowMap)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		
		SetDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);

		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);

		SetShaderValue(RHICmdList, ShaderRHI, PhaseG, FogInfo.VolumetricFogScatteringDistribution);
		SetShaderValue(RHICmdList, ShaderRHI, InverseSquaredLightDistanceBiasScale, GInverseSquaredLightDistanceBiasScale);

		VolumeShadowingParameters.Set(RHICmdList, ShaderRHI, View, LightSceneInfo, ShadowMap, 0, bDynamicallyShadowed);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PhaseG;
		Ar << InverseSquaredLightDistanceBiasScale;
		Ar << VolumetricFogParameters;
		Ar << VolumeShadowingParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter PhaseG;
	FShaderParameter InverseSquaredLightDistanceBiasScale;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FVolumeShadowingParameters VolumeShadowingParameters;
};

#define IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(bDynamicallyShadowed,bInverseSquared,bTemporalReprojection) \
	typedef TInjectShadowedLocalLightPS<bDynamicallyShadowed,bInverseSquared,bTemporalReprojection> TInjectShadowedLocalLightPS##bDynamicallyShadowed##bInverseSquared##bTemporalReprojection; \
	IMPLEMENT_SHADER_TYPE(template<>,TInjectShadowedLocalLightPS##bDynamicallyShadowed##bInverseSquared##bTemporalReprojection,TEXT("/Engine/Private/VolumetricFog.usf"),TEXT("InjectShadowedLocalLightPS"),SF_Pixel);

IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(true, true, true);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(true, false, true);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(false, true, true);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(false, false, true);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(true, true, false);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(true, false, false);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(false, true, false);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(false, false, false);

FProjectedShadowInfo* GetShadowForInjectionIntoVolumetricFog(const FLightSceneProxy* LightProxy, FVisibleLightInfo& VisibleLightInfo)
{
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

		if (ProjectedShadowInfo->bAllocated
			&& ProjectedShadowInfo->bWholeSceneShadow
			&& !ProjectedShadowInfo->bRayTracedDistanceField)
		{
			return ProjectedShadowInfo;
		}
	}

	return NULL;
}

bool LightNeedsSeparateInjectionIntoVolumetricFog(const FLightSceneInfo* LightSceneInfo, FVisibleLightInfo& VisibleLightInfo)
{
	const FLightSceneProxy* LightProxy = LightSceneInfo->Proxy;

	if (GVolumetricFogInjectShadowedLightsSeparately 
		&& (LightProxy->GetLightType() == LightType_Point || LightProxy->GetLightType() == LightType_Spot)
		&& !LightProxy->HasStaticLighting() 
		&& LightProxy->CastsDynamicShadow()
		&& LightProxy->CastsVolumetricShadow())
	{
		const FStaticShadowDepthMap* StaticShadowDepthMap = LightProxy->GetStaticShadowDepthMap();
		const bool bStaticallyShadowed = LightSceneInfo->IsPrecomputedLightingValid() && StaticShadowDepthMap && StaticShadowDepthMap->TextureRHI;

		return GetShadowForInjectionIntoVolumetricFog(LightProxy, VisibleLightInfo) != NULL || bStaticallyShadowed;
	}

	return false;
}

FIntPoint CalculateVolumetricFogBoundsForLight(const FSphere& LightBounds, const FViewInfo& View, FIntVector VolumetricFogGridSize, FVector GridZParams)
{
	FIntPoint VolumeZBounds;

	FVector ViewSpaceLightBoundsOrigin = View.ViewMatrices.GetViewMatrix().TransformPosition(LightBounds.Center);

	int32 FurthestSliceIndexUnclamped = ComputeZSliceFromDepth(ViewSpaceLightBoundsOrigin.Z + LightBounds.W, GridZParams);
	int32 ClosestSliceIndexUnclamped = ComputeZSliceFromDepth(ViewSpaceLightBoundsOrigin.Z - LightBounds.W, GridZParams);

	VolumeZBounds.X = FMath::Clamp(ClosestSliceIndexUnclamped, 0, VolumetricFogGridSize.Z - 1);
	VolumeZBounds.Y = FMath::Clamp(FurthestSliceIndexUnclamped, 0, VolumetricFogGridSize.Z - 1);

	return VolumeZBounds;
}

template<bool bDynamicallyShadowed, bool bInverseSquared, bool bUseTemporalReprojection>
void SetInjectShadowedLocalLightShaders(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View, 
	const FVolumetricFogIntegrationParameterData& IntegrationData,
	const FLightSceneInfo* LightSceneInfo,
	const FSphere& LightBounds,
	const FExponentialHeightFogSceneInfo& FogInfo,
	const FProjectedShadowInfo* ProjectedShadowInfo,
	FIntVector VolumetricFogGridSize,
	int32 MinZ)
{
	TShaderMapRef<FWriteToBoundingSphereVS> VertexShader(View.ShaderMap);
	TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
	TShaderMapRef<TInjectShadowedLocalLightPS<bDynamicallyShadowed, bInverseSquared, bUseTemporalReprojection>> PixelShader(View.ShaderMap);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	// Accumulate the contribution of multiple lights
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(RHICmdList, View, IntegrationData, LightSceneInfo, FogInfo, ProjectedShadowInfo);
	VertexShader->SetParameters(RHICmdList, View, IntegrationData, LightBounds, MinZ);

	if (GeometryShader.IsValid())
	{
		GeometryShader->SetParameters(RHICmdList, MinZ);
	}
}

/**  */
class FCircleRasterizeVertexBuffer : public FVertexBuffer
{
public:

	virtual void InitRHI() override
	{
		const int32 NumTriangles = NumVertices - 2;
		const uint32 Size = NumVertices * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, Buffer);		
		FScreenVertex* DestVertex = (FScreenVertex*)Buffer;

		const int32 NumRings = NumVertices;
		const float RadiansPerRingSegment = PI / (float)NumRings;

		// Boost the effective radius so that the edges of the circle approximation lie on the circle, instead of the vertices
		const float RadiusScale = 1.0f / FMath::Cos(RadiansPerRingSegment);

		for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
		{
			float Angle = VertexIndex / (float)(NumVertices - 1) * 2 * PI;
			// WriteToBoundingSphereVS only uses UV
			DestVertex[VertexIndex].Position = FVector2D(0, 0);
			DestVertex[VertexIndex].UV = FVector2D(RadiusScale * FMath::Cos(Angle) * .5f + .5f, RadiusScale * FMath::Sin(Angle) * .5f + .5f);
		}

		RHIUnlockVertexBuffer(VertexBufferRHI);      
	}

	static int32 NumVertices;
};

int32 FCircleRasterizeVertexBuffer::NumVertices = 8;

TGlobalResource<FCircleRasterizeVertexBuffer> GCircleRasterizeVertexBuffer;

/**  */
class FCircleRasterizeIndexBuffer : public FIndexBuffer
{
public:

	virtual void InitRHI() override
	{
		const int32 NumTriangles = FCircleRasterizeVertexBuffer::NumVertices - 2;

		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;
		Indices.Empty(NumTriangles * 3);

		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
		{
			int32 LeadingVertexIndex = TriangleIndex + 2;
			Indices.Add(0);
			Indices.Add(LeadingVertexIndex - 1);
			Indices.Add(LeadingVertexIndex);
		}

		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(uint16);

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Indices);
		IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}
};

TGlobalResource<FCircleRasterizeIndexBuffer> GCircleRasterizeIndexBuffer;

void FDeferredShadingSceneRenderer::RenderLocalLightsForVolumetricFog(
	FRHICommandListImmediate& RHICmdList,
	FViewInfo& View,
	bool bUseTemporalReprojection,
	const FVolumetricFogIntegrationParameterData& IntegrationData,
	const FExponentialHeightFogSceneInfo& FogInfo,
	FIntVector VolumetricFogGridSize, 
	FVector GridZParams,
	const FPooledRenderTargetDesc& VolumeDesc,
	TRefCountPtr<IPooledRenderTarget>& OutLocalShadowedLightScattering)
{
	TArray<const FLightSceneInfo*, SceneRenderingAllocator> LightsToInject;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent() 
			&& LightSceneInfo->ShouldRenderLight(View)
			&& LightNeedsSeparateInjectionIntoVolumetricFog(LightSceneInfo, VisibleLightInfos[LightSceneInfo->Id])
			&& LightSceneInfo->Proxy->GetVolumetricScatteringIntensity() > 0)
		{
			const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

			if ((View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < (FogInfo.VolumetricFogDistance + LightBounds.W) * (FogInfo.VolumetricFogDistance + LightBounds.W))
			{
				LightsToInject.Add(LightSceneInfo);
			}
		}
	}

	if (LightsToInject.Num() > 0)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ShadowedLights);

		GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, OutLocalShadowedLightScattering, TEXT("LocalShadowedLightScattering"));

		FRHIRenderTargetView ColorView(OutLocalShadowedLightScattering->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
		FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView());
		RHICmdList.SetRenderTargetsAndClear(Info);

		for (int32 LightIndex = 0; LightIndex < LightsToInject.Num(); LightIndex++)
		{
			const FLightSceneInfo* LightSceneInfo = LightsToInject[LightIndex];
			FProjectedShadowInfo* ProjectedShadowInfo = GetShadowForInjectionIntoVolumetricFog(LightSceneInfo->Proxy, VisibleLightInfos[LightSceneInfo->Id]);

			const bool bInverseSquared = LightSceneInfo->Proxy->IsInverseSquared();
			const bool bDynamicallyShadowed = ProjectedShadowInfo != NULL;
			const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
			const FIntPoint VolumeZBounds = CalculateVolumetricFogBoundsForLight(LightBounds, View, VolumetricFogGridSize, GridZParams);

			if (VolumeZBounds.X < VolumeZBounds.Y)
			{
				if (bUseTemporalReprojection)
				{
					if (bDynamicallyShadowed)
					{
						if (bInverseSquared)
						{
							SetInjectShadowedLocalLightShaders<true, true, true>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
						else
						{
							SetInjectShadowedLocalLightShaders<true, false, true>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
					}
					else
					{
						if (bInverseSquared)
						{
							SetInjectShadowedLocalLightShaders<false, true, true>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
						else
						{
							SetInjectShadowedLocalLightShaders<false, false, true>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
					}
				}
				else
				{
					if (bDynamicallyShadowed)
					{
						if (bInverseSquared)
						{
							SetInjectShadowedLocalLightShaders<true, true, false>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
						else
						{
							SetInjectShadowedLocalLightShaders<true, false, false>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
					}
					else
					{
						if (bInverseSquared)
						{
							SetInjectShadowedLocalLightShaders<false, true, false>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
						else
						{
							SetInjectShadowedLocalLightShaders<false, false, false>(RHICmdList, View, IntegrationData, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
						}
					}
				}

				RHICmdList.SetStreamSource(0, GCircleRasterizeVertexBuffer.VertexBufferRHI, 0);
				const int32 NumInstances = VolumeZBounds.Y - VolumeZBounds.X;
				const int32 NumTriangles = FCircleRasterizeVertexBuffer::NumVertices - 2;
				RHICmdList.DrawIndexedPrimitive(GCircleRasterizeIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, FCircleRasterizeVertexBuffer::NumVertices, 0, NumTriangles, NumInstances);
			}
		}

		RHICmdList.CopyToResolveTarget(OutLocalShadowedLightScattering->GetRenderTargetItem().TargetableTexture, OutLocalShadowedLightScattering->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());
	
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, OutLocalShadowedLightScattering);
	}
}

template<bool bTemporalReprojection, bool bDistanceFieldSkyOcclusion>
class TVolumetricFogLightScatteringCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TVolumetricFogLightScatteringCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogGridInjectionGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_TEMPORAL_REPROJECTION"), bTemporalReprojection);
		OutEnvironment.SetDefine(TEXT("DISTANCE_FIELD_SKY_OCCLUSION"), bDistanceFieldSkyOcclusion);
	}

	TVolumetricFogLightScatteringCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		LocalShadowedLightScattering.Bind(Initializer.ParameterMap, TEXT("LocalShadowedLightScattering"));
		LightScatteringHistory.Bind(Initializer.ParameterMap, TEXT("LightScatteringHistory"));
		LightScatteringHistorySampler.Bind(Initializer.ParameterMap, TEXT("LightScatteringHistorySampler"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
		DirectionalLightFunctionWorldToShadow.Bind(Initializer.ParameterMap, TEXT("DirectionalLightFunctionWorldToShadow"));
		LightFunctionTexture.Bind(Initializer.ParameterMap, TEXT("LightFunctionTexture"));
		LightFunctionSampler.Bind(Initializer.ParameterMap, TEXT("LightFunctionSampler"));
		StaticLightingScatteringIntensity.Bind(Initializer.ParameterMap, TEXT("StaticLightingScatteringIntensity"));
		SkyLightUseStaticShadowing.Bind(Initializer.ParameterMap, TEXT("SkyLightUseStaticShadowing"));
		SkyLightVolumetricScatteringIntensity.Bind(Initializer.ParameterMap, TEXT("SkyLightVolumetricScatteringIntensity"));
		SkySH.Bind(Initializer.ParameterMap, TEXT("SkySH"));
		PhaseG.Bind(Initializer.ParameterMap, TEXT("PhaseG"));
		InverseSquaredLightDistanceBiasScale.Bind(Initializer.ParameterMap, TEXT("InverseSquaredLightDistanceBiasScale"));
		UseHeightFogColors.Bind(Initializer.ParameterMap, TEXT("UseHeightFogColors"));
		UseDirectionalLightShadowing.Bind(Initializer.ParameterMap, TEXT("UseDirectionalLightShadowing"));
		AOParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
	}

	TVolumetricFogLightScatteringCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FVolumetricFogIntegrationParameterData& IntegrationData,
		const FExponentialHeightFogSceneInfo& FogInfo,
		IPooledRenderTarget* LocalShadowedLightScatteringTarget,
		FTextureRHIParamRef LightScatteringHistoryTexture,
		bool bUseDirectionalLightShadowing,
		const FMatrix& DirectionalLightFunctionWorldToShadowValue,
		TRefCountPtr<IPooledRenderTarget>& LightFunctionTextureValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		FTextureRHIParamRef LocalShadowedLightScatteringTexture = GBlackVolumeTexture->TextureRHI;

		if (LocalShadowedLightScatteringTarget)
		{
			LocalShadowedLightScatteringTexture = LocalShadowedLightScatteringTarget->GetRenderTargetItem().ShaderResourceTexture;
		}

		SetTextureParameter(RHICmdList, ShaderRHI, LocalShadowedLightScattering, LocalShadowedLightScatteringTexture);

		if (!LightScatteringHistoryTexture)
		{
			LightScatteringHistoryTexture = GBlackVolumeTexture->TextureRHI;
		}

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI, 
			LightScatteringHistory, 
			LightScatteringHistorySampler, 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			LightScatteringHistoryTexture);

		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);
		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, DirectionalLightFunctionWorldToShadow, DirectionalLightFunctionWorldToShadowValue);

		FTextureRHIParamRef LightFunctionRHITexture = GWhiteTexture->TextureRHI;

		if (LightFunctionTextureValue)
		{
			LightFunctionRHITexture = LightFunctionTextureValue->GetRenderTargetItem().ShaderResourceTexture;
		}

		SetTextureParameter(RHICmdList, ShaderRHI, LightFunctionTexture, LightFunctionSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), LightFunctionRHITexture);

		FScene* Scene = (FScene*)View.Family->Scene;
		FDistanceFieldAOParameters AOParameterData(Scene->DefaultMaxDistanceFieldOcclusionDistance);
		FSkyLightSceneProxy* SkyLight = Scene->SkyLight;

		if (SkyLight
			// Skylights with static lighting had their diffuse contribution baked into lightmaps
			&& !SkyLight->bHasStaticLighting
			&& View.Family->EngineShowFlags.SkyLighting)
		{
			const float LocalSkyLightUseStaticShadowing = SkyLight->bWantsStaticShadowing && SkyLight->bCastShadows ? 1.0f : 0.0f;
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightUseStaticShadowing, LocalSkyLightUseStaticShadowing);
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightVolumetricScatteringIntensity, SkyLight->VolumetricScatteringIntensity);

			const FSHVectorRGB3& SkyIrradiance = SkyLight->IrradianceEnvironmentMap;
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.R.V, 0);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.G.V, 1);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.B.V, 2);

			AOParameterData = FDistanceFieldAOParameters(SkyLight->OcclusionMaxDistance, SkyLight->Contrast);
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightUseStaticShadowing, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightVolumetricScatteringIntensity, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 0);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 1);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 2);
		}

		float StaticLightingScatteringIntensityValue = 0;

		if (View.Family->EngineShowFlags.GlobalIllumination && View.Family->EngineShowFlags.VolumetricLightmap)
		{
			StaticLightingScatteringIntensityValue = FogInfo.VolumetricFogStaticLightingScatteringIntensity;
		}

		SetShaderValue(RHICmdList, ShaderRHI, StaticLightingScatteringIntensity, StaticLightingScatteringIntensityValue);

		SetShaderValue(RHICmdList, ShaderRHI, PhaseG, FogInfo.VolumetricFogScatteringDistribution);
		SetShaderValue(RHICmdList, ShaderRHI, InverseSquaredLightDistanceBiasScale, GInverseSquaredLightDistanceBiasScale);
		SetShaderValue(RHICmdList, ShaderRHI, UseHeightFogColors, FogInfo.bOverrideLightColorsWithFogInscatteringColors ? 1.0f : 0.0f);
		SetShaderValue(RHICmdList, ShaderRHI, UseDirectionalLightShadowing, bUseDirectionalLightShadowing ? 1.0f : 0.0f);

		AOParameters.Set(RHICmdList, ShaderRHI, AOParameterData);
		GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, View.GlobalDistanceFieldInfo.ParameterData);
		HeightFogParameters.Set(RHICmdList, ShaderRHI, &View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, IPooledRenderTarget* LightScatteringRenderTarget)
	{
		VolumetricFogParameters.UnsetParameters(RHICmdList, GetComputeShader(), View, NULL, NULL, LightScatteringRenderTarget, true);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LocalShadowedLightScattering;
		Ar << LightScatteringHistory;
		Ar << LightScatteringHistorySampler;
		Ar << VolumetricFogParameters;
		Ar << ForwardLightingParameters;
		Ar << DirectionalLightFunctionWorldToShadow;
		Ar << LightFunctionTexture;
		Ar << LightFunctionSampler;
		Ar << StaticLightingScatteringIntensity;
		Ar << SkyLightUseStaticShadowing;
		Ar << SkyLightVolumetricScatteringIntensity;
		Ar << SkySH;
		Ar << PhaseG;
		Ar << InverseSquaredLightDistanceBiasScale;
		Ar << UseHeightFogColors;
		Ar << UseDirectionalLightShadowing;
		Ar << AOParameters;
		Ar << GlobalDistanceFieldParameters;
		Ar << HeightFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter LocalShadowedLightScattering;
	FShaderResourceParameter LightScatteringHistory;
	FShaderResourceParameter LightScatteringHistorySampler;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FForwardLightingParameters ForwardLightingParameters;
	FShaderParameter DirectionalLightFunctionWorldToShadow;
	FShaderResourceParameter LightFunctionTexture;
	FShaderResourceParameter LightFunctionSampler;
	FShaderParameter StaticLightingScatteringIntensity;
	FShaderParameter SkyLightUseStaticShadowing;
	FShaderParameter SkyLightVolumetricScatteringIntensity;
	FShaderParameter SkySH;
	FShaderParameter PhaseG;
	FShaderParameter InverseSquaredLightDistanceBiasScale;
	FShaderParameter UseHeightFogColors;
	FShaderParameter UseDirectionalLightShadowing;
	FAOParameters AOParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
	FExponentialHeightFogShaderParameters HeightFogParameters;
};

#define IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(bTemporalReprojection, bDistanceFieldSkyOcclusion) \
	typedef TVolumetricFogLightScatteringCS<bTemporalReprojection, bDistanceFieldSkyOcclusion> TVolumetricFogLightScatteringCS##bTemporalReprojection##bDistanceFieldSkyOcclusion; \
	IMPLEMENT_SHADER_TYPE(template<>,TVolumetricFogLightScatteringCS##bTemporalReprojection##bDistanceFieldSkyOcclusion,TEXT("/Engine/Private/VolumetricFog.usf"),TEXT("LightScatteringCS"),SF_Compute);

IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(true, true)
IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(false, true)
IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(true, false)
IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(false, false)

uint32 VolumetricFogIntegrationGroupSize = 8;

class FVolumetricFogFinalIntegrationCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVolumetricFogFinalIntegrationCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogIntegrationGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FVolumetricFogFinalIntegrationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
	}

	FVolumetricFogFinalIntegrationCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FVolumetricFogIntegrationParameterData& IntegrationData)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		VolumetricFogParameters.UnsetParameters(RHICmdList, GetComputeShader(), View, NULL, NULL, NULL, true);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumetricFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FVolumetricFogIntegrationParameters VolumetricFogParameters;
};

IMPLEMENT_SHADER_TYPE(,FVolumetricFogFinalIntegrationCS,TEXT("/Engine/Private/VolumetricFog.usf"),TEXT("FinalIntegrationCS"),SF_Compute);

bool ShouldRenderVolumetricFog(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return ShouldRenderFog(ViewFamily)
		&& Scene
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5 
		&& DoesPlatformSupportVolumetricFog(Scene->GetShaderPlatform())
		&& GVolumetricFog
		&& ViewFamily.EngineShowFlags.VolumetricFog
		&& Scene->ExponentialFogs.Num() > 0
		&& Scene->ExponentialFogs[0].bEnableVolumetricFog
		&& Scene->ExponentialFogs[0].VolumetricFogDistance > 0;
}

FVector GetVolumetricFogGridZParams(float NearPlane, float FarPlane, int32 GridSizeZ)
{
	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spend lots of resolution right in front of the near plane
	double NearOffset = .095 * 100;
	// Space out the slices so they aren't all clustered at the near plane
	double S = GVolumetricFogDepthDistributionScale;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * FMath::Exp2((GridSizeZ - 1) / S)) / (F - N);
	double B = (1 - O) / N;

	double O2 = (FMath::Exp2((GridSizeZ - 1) / S) - F / N) / (-F / N + 1);

	float FloatN = (float)N;
	float FloatF = (float)F;
	float FloatB = (float)B;
	float FloatO = (float)O;
	float FloatS = (float)S;

	float NSlice = FMath::Log2(FloatN*FloatB + FloatO) * FloatS;
	float NearPlaneSlice = FMath::Log2(NearPlane*FloatB + FloatO) * FloatS;
	float FSlice = FMath::Log2(FloatF*FloatB + FloatO) * FloatS;
	// y = log2(z*B + O) * S
	// f(N) = 0 = log2(N*B + O) * S
	// 1 = N*B + O
	// O = 1 - N*B
	// B = (1 - O) / N

	// f(F) = GLightGridSizeZ - 1 = log2(F*B + O) * S
	// exp2((GLightGridSizeZ - 1) / S) = F*B + O
	// exp2((GLightGridSizeZ - 1) / S) = F * (1 - O) / N + O
	// exp2((GLightGridSizeZ - 1) / S) = F / N - F / N * O + O
	// exp2((GLightGridSizeZ - 1) / S) = F / N + (-F / N + 1) * O
	// O = (exp2((GLightGridSizeZ - 1) / S) - F / N) / (-F / N + 1)

	return FVector(B, O, S);
}

FIntVector GetVolumetricFogGridSize(FIntPoint ViewRectSize)
{
	extern int32 GLightGridSizeZ;
	const FIntPoint VolumetricFogGridSizeXY = FIntPoint::DivideAndRoundUp(ViewRectSize, GVolumetricFogGridPixelSize);
	return FIntVector(VolumetricFogGridSizeXY.X, VolumetricFogGridSizeXY.Y, GVolumetricFogGridSizeZ);
}

void FViewInfo::SetupVolumetricFogUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	const FScene* Scene = (const FScene*)Family->Scene;

	if (ShouldRenderVolumetricFog(Scene, *Family))
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(ViewRect.Size());
			
		ViewUniformShaderParameters.VolumetricFogInvGridSize = FVector(1.0f / VolumetricFogGridSize.X, 1.0f / VolumetricFogGridSize.Y, 1.0f / VolumetricFogGridSize.Z);

		const FVector ZParams = GetVolumetricFogGridZParams(NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);
		ViewUniformShaderParameters.VolumetricFogGridZParams = ZParams;

		ViewUniformShaderParameters.VolumetricFogSVPosToVolumeUV = FVector2D(1.0f, 1.0f) / (FVector2D(VolumetricFogGridSize.X, VolumetricFogGridSize.Y) * GVolumetricFogGridPixelSize);
		ViewUniformShaderParameters.VolumetricFogMaxDistance = FogInfo.VolumetricFogDistance;
	}
	else
	{
		ViewUniformShaderParameters.VolumetricFogInvGridSize = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricFogGridZParams = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricFogSVPosToVolumeUV = FVector2D(0, 0);
		ViewUniformShaderParameters.VolumetricFogMaxDistance = 0;
	}
}

bool FDeferredShadingSceneRenderer::ShouldRenderVolumetricFog() const
{
	return ::ShouldRenderVolumetricFog(Scene, ViewFamily);
}

void FDeferredShadingSceneRenderer::SetupVolumetricFog()
{
	if (ShouldRenderVolumetricFog())
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(View.ViewRect.Size());
			
			FVolumetricFogGlobalData GlobalData;
			GlobalData.GridSizeInt = VolumetricFogGridSize;
			GlobalData.GridSize = FVector(VolumetricFogGridSize);

			FVector ZParams = GetVolumetricFogGridZParams(View.NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);
			GlobalData.GridZParams = ZParams;

			GlobalData.SVPosToVolumeUV = FVector2D(1.0f, 1.0f) / (FVector2D(GlobalData.GridSize) * GVolumetricFogGridPixelSize);
			GlobalData.FogGridToPixelXY = FIntPoint(GVolumetricFogGridPixelSize, GVolumetricFogGridPixelSize);
			GlobalData.MaxDistance = FogInfo.VolumetricFogDistance;

			GlobalData.HeightFogInscatteringColor = View.ExponentialFogColor;

			GlobalData.HeightFogDirectionalLightInscatteringColor = FVector::ZeroVector;

			if (View.bUseDirectionalInscattering && !View.FogInscatteringColorCubemap)
			{
				GlobalData.HeightFogDirectionalLightInscatteringColor = FVector(View.DirectionalInscatteringColor);
			}

			View.VolumetricFogResources.VolumetricFogGlobalData = TUniformBufferRef<FVolumetricFogGlobalData>::CreateUniformBufferImmediate(GlobalData, UniformBuffer_SingleFrame);
		}
	}
	else
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			if (View.ViewState)
			{
				View.ViewState->LightScatteringHistory = NULL;
			}
		}
	}
}

void FDeferredShadingSceneRenderer::ComputeVolumetricFog(FRHICommandListImmediate& RHICmdList)
{
	if (ShouldRenderVolumetricFog())
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(View.ViewRect.Size());
			const FVector GridZParams = GetVolumetricFogGridZParams(View.NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);

			SCOPED_DRAW_EVENT(RHICmdList, VolumetricFog);

			const FVector FrameJitterOffsetValue = VolumetricFogTemporalRandom(View.Family->FrameNumber);

			FVolumetricFogIntegrationParameterData IntegrationData;
			IntegrationData.FrameJitterOffsetValues.Empty(16);
			IntegrationData.FrameJitterOffsetValues.AddZeroed(16);
			IntegrationData.FrameJitterOffsetValues[0] = VolumetricFogTemporalRandom(View.Family->FrameNumber);

			for (int32 FrameOffsetIndex = 1; FrameOffsetIndex < GVolumetricFogHistoryMissSupersampleCount; FrameOffsetIndex++)
			{
				IntegrationData.FrameJitterOffsetValues[FrameOffsetIndex] = VolumetricFogTemporalRandom(View.Family->FrameNumber - FrameOffsetIndex);
			}

			const bool bUseTemporalReprojection = 
				GVolumetricFogTemporalReprojection 
				&& View.ViewState;

			IntegrationData.bTemporalHistoryIsValid =
				bUseTemporalReprojection
				&& !View.bCameraCut
				&& !View.bPrevTransformsReset
				&& ViewFamily.bRealtimeUpdate
				&& View.ViewState->LightScatteringHistory
				&& View.ViewState->LightScatteringHistory->GetDesc().GetSize() == VolumetricFogGridSize;

			FMatrix LightFunctionWorldToShadow;
			TRefCountPtr<IPooledRenderTarget> LightFunctionTexture;
			bool bUseDirectionalLightShadowing;

			RenderLightFunctionForVolumetricFog(
				RHICmdList,
				View,
				VolumetricFogGridSize,
				FogInfo.VolumetricFogDistance,
				LightFunctionWorldToShadow,
				LightFunctionTexture,
				bUseDirectionalLightShadowing);

			TRefCountPtr<IPooledRenderTarget> VBufferA;
			TRefCountPtr<IPooledRenderTarget> VBufferB;

			const uint32 Flags = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV | TexCreate_ReduceMemoryWithTilingMode;
			FPooledRenderTargetDesc VolumeDesc(FPooledRenderTargetDesc::CreateVolumeDesc(VolumetricFogGridSize.X, VolumetricFogGridSize.Y, VolumetricFogGridSize.Z, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_None, Flags, false));
			FPooledRenderTargetDesc VolumeDescFastVRAM = VolumeDesc;
			VolumeDescFastVRAM.Flags |= GFastVRamConfig.VolumetricFog;
			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDescFastVRAM, VBufferA, TEXT("VBufferA"));
			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDescFastVRAM, VBufferB, TEXT("VBufferB"));

			IntegrationData.VBufferARenderTarget = VBufferA.GetReference();
			IntegrationData.VBufferBRenderTarget = VBufferB.GetReference();

			// Unbind render targets, the shadow depth target may still be bound
			SetRenderTarget(RHICmdList, NULL, NULL);

			{
				const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogGridInjectionGroupSize);

				{
					SCOPED_DRAW_EVENT(RHICmdList, InitializeVolumeAttributes);
					TShaderMapRef<FVolumetricFogMaterialSetupCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, IntegrationData, FogInfo);
					DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					ComputeShader->UnsetParameters(RHICmdList, View, VBufferA.GetReference(), VBufferB.GetReference());
				}

				FUnorderedAccessViewRHIParamRef VoxelizeUAVs[2] =
				{
					VBufferA->GetRenderTargetItem().UAV.GetReference(),
					VBufferB->GetRenderTargetItem().UAV.GetReference()
				};

				RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EComputeToGfx, VoxelizeUAVs, ARRAY_COUNT(VoxelizeUAVs), nullptr);

				VoxelizeFogVolumePrimitives(
					RHICmdList, 
					View, 
					IntegrationData,
					VolumetricFogGridSize, 
					GridZParams,
					FogInfo.VolumetricFogDistance);

				FTextureRHIParamRef VoxelizeRenderTargets[2] =
				{
					VBufferA->GetRenderTargetItem().TargetableTexture,
					VBufferB->GetRenderTargetItem().TargetableTexture
				};

				RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, VoxelizeRenderTargets, ARRAY_COUNT(VoxelizeRenderTargets));

				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, VBufferA);
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, VBufferB);
			}

			TRefCountPtr<IPooledRenderTarget> LocalShadowedLightScattering = NULL;
			RenderLocalLightsForVolumetricFog(RHICmdList, View, bUseTemporalReprojection, IntegrationData, FogInfo, VolumetricFogGridSize, GridZParams, VolumeDescFastVRAM, LocalShadowedLightScattering);

			TRefCountPtr<IPooledRenderTarget> LightScattering;
			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, LightScattering, TEXT("LightScattering"));

			IntegrationData.LightScatteringRenderTarget = LightScattering.GetReference();

			SetRenderTarget(RHICmdList, NULL, NULL);

			{
				const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogGridInjectionGroupSize);

				const bool bUseGlobalDistanceField = UseGlobalDistanceField() && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0;

				const bool bUseDistanceFieldSkyOcclusion =
					ViewFamily.EngineShowFlags.AmbientOcclusion
					&& Scene->SkyLight
					&& Scene->SkyLight->bCastShadows 
					&& Scene->SkyLight->bCastVolumetricShadow
					&& ShouldRenderDistanceFieldAO() 
					&& SupportsDistanceFieldAO(View.GetFeatureLevel(), View.GetShaderPlatform())
					&& bUseGlobalDistanceField
					&& Views.Num() == 1
					&& View.IsPerspectiveProjection();

				SCOPED_DRAW_EVENTF(RHICmdList, LightScattering, TEXT("LightScattering %dx%dx%d %s %s"), 
					VolumetricFogGridSize.X, 
					VolumetricFogGridSize.Y, 
					VolumetricFogGridSize.Z,
					bUseDistanceFieldSkyOcclusion ? TEXT("DFAO") : TEXT(""),
					LightFunctionTexture ? TEXT("LF") : TEXT(""));

				if (bUseTemporalReprojection)
				{
					FTextureRHIParamRef LightScatteringHistoryTexture = View.ViewState->LightScatteringHistory 
						? View.ViewState->LightScatteringHistory->GetRenderTargetItem().ShaderResourceTexture
						: GBlackVolumeTexture->TextureRHI;

					if (bUseDistanceFieldSkyOcclusion)
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<true, true> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, IntegrationData, FogInfo, LocalShadowedLightScattering.GetReference(), LightScatteringHistoryTexture, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
					else
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<true, false> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, IntegrationData, FogInfo, LocalShadowedLightScattering.GetReference(), LightScatteringHistoryTexture, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
				}
				else
				{
					if (bUseDistanceFieldSkyOcclusion)
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<false, true> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, IntegrationData, FogInfo, LocalShadowedLightScattering.GetReference(), NULL, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
					else
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<false, false> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, IntegrationData, FogInfo, LocalShadowedLightScattering.GetReference(), NULL, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
				}

				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, LightScattering);

				if (bUseTemporalReprojection)
				{
					View.ViewState->LightScatteringHistory = LightScattering;
				}
				else if (View.ViewState)
				{
					View.ViewState->LightScatteringHistory = NULL;
				}
			}

			VBufferA = NULL;
			VBufferB = NULL;
			LightFunctionTexture = NULL;

			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, View.VolumetricFogResources.IntegratedLightScattering, TEXT("IntegratedLightScattering"));

			{
				SCOPED_DRAW_EVENT(RHICmdList, FinalIntegration);

				const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogIntegrationGroupSize);
				TShaderMapRef<FVolumetricFogFinalIntegrationCS> ComputeShader(View.ShaderMap);
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, IntegrationData);
				DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, 1);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, View.VolumetricFogResources.IntegratedLightScattering);
		}
	}
}
