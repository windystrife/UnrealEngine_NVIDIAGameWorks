// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldGlobalIllumination.cpp
=============================================================================*/

#include "DistanceFieldGlobalIllumination.h"
#include "DistanceFieldLightingShared.h"
#include "UniquePtr.h"
#include "ClearQuad.h"

int32 GDistanceFieldGI = 0;
FAutoConsoleVariableRef CVarDistanceFieldGI(
	TEXT("r.DistanceFieldGI"),
	GDistanceFieldGI,
	TEXT(""),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

int32 GVPLMeshGlobalIllumination = 1;
FAutoConsoleVariableRef CVarVPLMeshGlobalIllumination(
	TEXT("r.VPLMeshGlobalIllumination"),
	GVPLMeshGlobalIllumination,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLSurfelRepresentation = 1;
FAutoConsoleVariableRef CVarVPLSurfelRepresentation(
	TEXT("r.VPLSurfelRepresentation"),
	GVPLSurfelRepresentation,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLGridDimension = 128;
FAutoConsoleVariableRef CVarVPLGridDimension(
	TEXT("r.VPLGridDimension"),
	GVPLGridDimension,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GVPLDirectionalLightTraceDistance = 100000;
FAutoConsoleVariableRef CVarVPLDirectionalLightTraceDistance(
	TEXT("r.VPLDirectionalLightTraceDistance"),
	GVPLDirectionalLightTraceDistance,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GVPLPlacementCameraRadius = 4000;
FAutoConsoleVariableRef CVarVPLPlacementCameraRadius(
	TEXT("r.VPLPlacementCameraRadius"),
	GVPLPlacementCameraRadius,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLViewCulling = 1;
FAutoConsoleVariableRef CVarVPLViewCulling(
	TEXT("r.VPLViewCulling"),
	GVPLViewCulling,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GAOUseConesForGI = 1;
FAutoConsoleVariableRef CVarAOUseConesForGI(
	TEXT("r.AOUseConesForGI"),
	GAOUseConesForGI,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLSpreadUpdateOver = 5;
FAutoConsoleVariableRef CVarVPLSpreadUpdateOver(
	TEXT("r.VPLSpreadUpdateOver"),
	GVPLSpreadUpdateOver,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GVPLSelfOcclusionReplacement = .3f;
FAutoConsoleVariableRef CVarVPLSelfOcclusionReplacement(
	TEXT("r.VPLSelfOcclusionReplacement"),
	GVPLSelfOcclusionReplacement,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

TGlobalResource<FVPLResources> GVPLResources;
TGlobalResource<FVPLResources> GCulledVPLResources;

extern TGlobalResource<FDistanceFieldObjectBufferResource> GShadowCulledObjectBuffers;

class FVPLPlacementCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVPLPlacementCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLightTileIntersectionParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	/** Default constructor. */
	FVPLPlacementCS() {}

	/** Initialization constructor. */
	FVPLPlacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("VPLParameterBuffer"));
		VPLData.Bind(Initializer.ParameterMap, TEXT("VPLData"));
		InvPlacementGridSize.Bind(Initializer.ParameterMap, TEXT("InvPlacementGridSize"));
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		ShadowToWorld.Bind(Initializer.ParameterMap, TEXT("ShadowToWorld"));
		LightDirectionAndTraceDistance.Bind(Initializer.ParameterMap, TEXT("LightDirectionAndTraceDistance"));
		LightColor.Bind(Initializer.ParameterMap, TEXT("LightColor"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		LightTileIntersectionParameters.Bind(Initializer.ParameterMap);
		VPLPlacementCameraRadius.Bind(Initializer.ParameterMap, TEXT("VPLPlacementCameraRadius"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FSceneView& View, 
		const FLightSceneProxy* LightSceneProxy,
		FVector2D InvPlacementGridSizeValue,
		const FMatrix& WorldToShadowValue,
		const FMatrix& ShadowToWorldValue,
		FLightTileIntersectionResources* TileIntersectionResources)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);

		SetShaderValue(RHICmdList, ShaderRHI, InvPlacementGridSize, InvPlacementGridSizeValue);
		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowValue);
		SetShaderValue(RHICmdList, ShaderRHI, ShadowToWorld, ShadowToWorldValue);
		SetShaderValue(RHICmdList, ShaderRHI, LightDirectionAndTraceDistance, FVector4(LightSceneProxy->GetDirection(), GVPLDirectionalLightTraceDistance));
		SetShaderValue(RHICmdList, ShaderRHI, LightColor, LightSceneProxy->GetColor() * LightSceneProxy->GetIndirectLightingScale());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = GVPLResources.VPLParameterBuffer.UAV;
		OutUAVs[1] = GVPLResources.VPLData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		VPLParameterBuffer.SetBuffer(RHICmdList, ShaderRHI, GVPLResources.VPLParameterBuffer);
		VPLData.SetBuffer(RHICmdList, ShaderRHI, GVPLResources.VPLData);

		check(TileIntersectionResources || !LightTileIntersectionParameters.IsBound());

		if (TileIntersectionResources)
		{
			LightTileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);
		}

		SetShaderValue(RHICmdList, ShaderRHI, VPLPlacementCameraRadius, GVPLPlacementCameraRadius);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		VPLParameterBuffer.UnsetUAV(RHICmdList, GetComputeShader());
		VPLData.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = GVPLResources.VPLParameterBuffer.UAV;
		OutUAVs[1] = GVPLResources.VPLData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VPLParameterBuffer;
		Ar << VPLData;
		Ar << InvPlacementGridSize;
		Ar << WorldToShadow;
		Ar << ShadowToWorld;
		Ar << LightDirectionAndTraceDistance;
		Ar << LightColor;
		Ar << ObjectParameters;
		Ar << LightTileIntersectionParameters;
		Ar << VPLPlacementCameraRadius;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter VPLParameterBuffer;
	FRWShaderParameter VPLData;
	FShaderParameter InvPlacementGridSize;
	FShaderParameter WorldToShadow;
	FShaderParameter ShadowToWorld;
	FShaderParameter LightDirectionAndTraceDistance;
	FShaderParameter LightColor;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FLightTileIntersectionParameters LightTileIntersectionParameters;
	FShaderParameter VPLPlacementCameraRadius;
};

IMPLEMENT_SHADER_TYPE(,FVPLPlacementCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("VPLPlacementCS"),SF_Compute);



class FSetupVPLCullndirectArgumentsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSetupVPLCullndirectArgumentsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	FSetupVPLCullndirectArgumentsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DispatchParameters.Bind(Initializer.ParameterMap, TEXT("DispatchParameters"));
		VPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("VPLParameterBuffer"));
	}

	FSetupVPLCullndirectArgumentsCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, GVPLResources.VPLDispatchIndirectBuffer.UAV);
		DispatchParameters.SetBuffer(RHICmdList, ShaderRHI, GVPLResources.VPLDispatchIndirectBuffer);
		SetSRVParameter(RHICmdList, ShaderRHI, VPLParameterBuffer, GVPLResources.VPLParameterBuffer.SRV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		DispatchParameters.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, GVPLResources.VPLDispatchIndirectBuffer.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DispatchParameters;
		Ar << VPLParameterBuffer;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter DispatchParameters;
	FShaderResourceParameter VPLParameterBuffer;
};

IMPLEMENT_SHADER_TYPE(,FSetupVPLCullndirectArgumentsCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("SetupVPLCullndirectArgumentsCS"),SF_Compute);

class FCullVPLsForViewCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullVPLsForViewCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	FCullVPLsForViewCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("VPLParameterBuffer"));
		VPLData.Bind(Initializer.ParameterMap, TEXT("VPLData"));
		CulledVPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("CulledVPLParameterBuffer"));
		CulledVPLData.Bind(Initializer.ParameterMap, TEXT("CulledVPLData"));
		AOParameters.Bind(Initializer.ParameterMap);
		NumConvexHullPlanes.Bind(Initializer.ParameterMap, TEXT("NumConvexHullPlanes"));
		ViewFrustumConvexHull.Bind(Initializer.ParameterMap, TEXT("ViewFrustumConvexHull"));
	}

	FCullVPLsForViewCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = GCulledVPLResources.VPLParameterBuffer.UAV;
		OutUAVs[1] = GCulledVPLResources.VPLData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		CulledVPLParameterBuffer.SetBuffer(RHICmdList, ShaderRHI, GCulledVPLResources.VPLParameterBuffer);
		CulledVPLData.SetBuffer(RHICmdList, ShaderRHI, GCulledVPLResources.VPLData);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		SetSRVParameter(RHICmdList, ShaderRHI, VPLParameterBuffer, GVPLResources.VPLParameterBuffer.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, VPLData, GVPLResources.VPLData.SRV);

		// Shader assumes max 6
		check(View.ViewFrustum.Planes.Num() <= 6);
		SetShaderValue(RHICmdList, ShaderRHI, NumConvexHullPlanes, View.ViewFrustum.Planes.Num());
		SetShaderValueArray(RHICmdList, ShaderRHI, ViewFrustumConvexHull, View.ViewFrustum.Planes.GetData(), View.ViewFrustum.Planes.Num());
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		CulledVPLParameterBuffer.UnsetUAV(RHICmdList, GetComputeShader());
		CulledVPLData.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VPLParameterBuffer;
		Ar << VPLData;
		Ar << CulledVPLParameterBuffer;
		Ar << CulledVPLData;
		Ar << AOParameters;
		Ar << NumConvexHullPlanes;
		Ar << ViewFrustumConvexHull;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter VPLParameterBuffer;
	FShaderResourceParameter VPLData;
	FRWShaderParameter CulledVPLParameterBuffer;
	FRWShaderParameter CulledVPLData;
	FAOParameters AOParameters;
	FShaderParameter NumConvexHullPlanes;
	FShaderParameter ViewFrustumConvexHull;
};

IMPLEMENT_SHADER_TYPE(,FCullVPLsForViewCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("CullVPLsForViewCS"),SF_Compute);

TUniquePtr<FLightTileIntersectionResources> GVPLPlacementTileIntersectionResources;

void PlaceVPLs(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FScene* Scene,
	const FDistanceFieldAOParameters& Parameters)
{
	GVPLResources.AllocateFor(GVPLGridDimension * GVPLGridDimension);

	{
		ClearUAV(RHICmdList, GVPLResources.VPLParameterBuffer, 0);
	}

	const FLightSceneProxy* DirectionalLightProxy = NULL;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent() 
			&& LightSceneInfo->Proxy->GetLightType() == LightType_Directional
			&& LightSceneInfo->Proxy->CastsDynamicShadow())
		{
			DirectionalLightProxy = LightSceneInfo->Proxy;
			break;
		}
	}

	if (DirectionalLightProxy)
	{
		SCOPED_DRAW_EVENT(RHICmdList, VPLPlacement);
		FMatrix DirectionalLightShadowToWorld;

		{
			int32 NumPlanes = 0;
			const FPlane* PlaneData = NULL;
			FVector4 ShadowBoundingSphereValue(0, 0, 0, 0);
			FShadowCascadeSettings CascadeSettings;
			FSphere ShadowBounds;
			FConvexVolume FrustumVolume;

			static bool bUseShadowmapBounds = true;

			if (bUseShadowmapBounds)
			{
				ShadowBounds = DirectionalLightProxy->GetShadowSplitBoundsDepthRange(
					View, 
					View.ViewMatrices.GetViewOrigin(),
					View.NearClippingDistance, 
					GVPLPlacementCameraRadius, 
					&CascadeSettings);

				FSphere SubjectBounds(FVector::ZeroVector, ShadowBounds.W);

				const FMatrix& WorldToLight = DirectionalLightProxy->GetWorldToLight();
				const FMatrix InitializerWorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
				const FVector InitializerFaceDirection = FVector(1,0,0);

				FVector	XAxis, YAxis;
				InitializerFaceDirection.FindBestAxisVectors(XAxis, YAxis);
				const FMatrix WorldToLightScaled = InitializerWorldToLight * FScaleMatrix(FVector(1.0f,1.0f / SubjectBounds.W,1.0f / SubjectBounds.W));
				const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,InitializerFaceDirection.GetSafeNormal(),FVector::ZeroVector);

				static bool bSnapPosition = true;

				if (bSnapPosition)
				{
					// Transform the shadow's position into shadowmap space
					const FVector TransformedPosition = WorldToFace.TransformPosition(ShadowBounds.Center);

					// Determine the distance necessary to snap the shadow's position to the nearest texel
					const float SnapX = FMath::Fmod(TransformedPosition.X, 2.0f / GVPLGridDimension);
					const float SnapY = FMath::Fmod(TransformedPosition.Y, 2.0f / GVPLGridDimension);
					// Snap the shadow's position and transform it back into world space
					// This snapping prevents sub-texel camera movements which removes view dependent aliasing from the final shadow result
					// This only maintains stable shadows under camera translation and rotation
					const FVector SnappedWorldPosition = WorldToFace.InverseFast().TransformPosition(TransformedPosition - FVector(SnapX, SnapY, 0.0f));
					ShadowBounds.Center = SnappedWorldPosition;
				}

				NumPlanes = CascadeSettings.ShadowBoundsAccurate.Planes.Num();
				PlaneData = CascadeSettings.ShadowBoundsAccurate.Planes.GetData();

				DirectionalLightShadowToWorld = FTranslationMatrix(-ShadowBounds.Center) * WorldToFace * FShadowProjectionMatrix(-GVPLDirectionalLightTraceDistance / 2, GVPLDirectionalLightTraceDistance / 2, FVector4(0,0,0,1));
			}
			else
			{
				ShadowBounds = FSphere(View.ViewMatrices.GetViewOrigin(), GVPLPlacementCameraRadius);

				FSphere SubjectBounds(FVector::ZeroVector, ShadowBounds.W);

				const FMatrix& WorldToLight = DirectionalLightProxy->GetWorldToLight();
				const FMatrix InitializerWorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
				const FVector InitializerFaceDirection = FVector(1,0,0);

				FVector	XAxis, YAxis;
				InitializerFaceDirection.FindBestAxisVectors(XAxis, YAxis);
				const FMatrix WorldToLightScaled = InitializerWorldToLight * FScaleMatrix(FVector(1.0f,1.0f / GVPLPlacementCameraRadius,1.0f / GVPLPlacementCameraRadius));
				const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,InitializerFaceDirection.GetSafeNormal(),FVector::ZeroVector);

				static bool bSnapPosition = true;

				if (bSnapPosition)
				{
					// Transform the shadow's position into shadowmap space
					const FVector TransformedPosition = WorldToFace.TransformPosition(ShadowBounds.Center);

					// Determine the distance necessary to snap the shadow's position to the nearest texel
					const float SnapX = FMath::Fmod(TransformedPosition.X, 2.0f / GVPLGridDimension);
					const float SnapY = FMath::Fmod(TransformedPosition.Y, 2.0f / GVPLGridDimension);
					// Snap the shadow's position and transform it back into world space
					// This snapping prevents sub-texel camera movements which removes view dependent aliasing from the final shadow result
					// This only maintains stable shadows under camera translation and rotation
					const FVector SnappedWorldPosition = WorldToFace.InverseFast().TransformPosition(TransformedPosition - FVector(SnapX, SnapY, 0.0f));
					ShadowBounds.Center = SnappedWorldPosition;
				}

				const float MaxSubjectZ = WorldToFace.TransformPosition(SubjectBounds.Center).Z + SubjectBounds.W;
				const float MinSubjectZ = FMath::Max(MaxSubjectZ - SubjectBounds.W * 2, (float)-HALF_WORLD_MAX);

				//@todo - naming is wrong and maybe derived matrices
				DirectionalLightShadowToWorld = FTranslationMatrix(-ShadowBounds.Center) * WorldToFace * FShadowProjectionMatrix(MinSubjectZ, MaxSubjectZ, FVector4(0,0,0,1));

				GetViewFrustumBounds(FrustumVolume, DirectionalLightShadowToWorld, true);

				NumPlanes = FrustumVolume.Planes.Num();
				PlaneData = FrustumVolume.Planes.GetData();
			}

			CullDistanceFieldObjectsForLight(
				RHICmdList,
				View,
				DirectionalLightProxy, 
				DirectionalLightShadowToWorld,
				NumPlanes,
				PlaneData,
				ShadowBoundingSphereValue,
				ShadowBounds.W,
				GVPLPlacementTileIntersectionResources);
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, PlaceVPLs);

			TShaderMapRef<FVPLPlacementCS> ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DirectionalLightProxy, FVector2D(1.0f / GVPLGridDimension, 1.0f / GVPLGridDimension), DirectionalLightShadowToWorld, DirectionalLightShadowToWorld.InverseFast(), GVPLPlacementTileIntersectionResources.Get());
			DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<int32>(GVPLGridDimension, GDistanceFieldAOTileSizeX), FMath::DivideAndRoundUp<int32>(GVPLGridDimension, GDistanceFieldAOTileSizeY), 1);

			ComputeShader->UnsetParameters(RHICmdList);
		}
		
		if (GVPLViewCulling)
		{
			{
				TShaderMapRef<FSetupVPLCullndirectArgumentsCS> ComputeShader(View.ShaderMap);
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View);

				DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
				ComputeShader->UnsetParameters(RHICmdList);
			}

			{
				GCulledVPLResources.AllocateFor(GVPLGridDimension * GVPLGridDimension);

				ClearUAV(RHICmdList, GCulledVPLResources.VPLParameterBuffer, 0);

				TShaderMapRef<FCullVPLsForViewCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, Scene, View, Parameters);

				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, GVPLResources.VPLDispatchIndirectBuffer.Buffer, 0);
				ComputeShader->UnsetParameters(RHICmdList);
			}
		}
	}
}

const int32 LightVPLsThreadGroupSize = 64;

class FSetupLightVPLsIndirectArgumentsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSetupLightVPLsIndirectArgumentsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("LIGHT_VPLS_THREADGROUP_SIZE"), LightVPLsThreadGroupSize);
	}

	FSetupLightVPLsIndirectArgumentsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DispatchParameters.Bind(Initializer.ParameterMap, TEXT("DispatchParameters"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		ObjectProcessStride.Bind(Initializer.ParameterMap, TEXT("ObjectProcessStride"));
	}

	FSetupLightVPLsIndirectArgumentsCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch.UAV);
		DispatchParameters.SetBuffer(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectProcessStride, GVPLSpreadUpdateOver);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		DispatchParameters.UnsetUAV(RHICmdList, ShaderRHI);

		ObjectParameters.UnsetParameters(RHICmdList, ShaderRHI);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DispatchParameters;
		Ar << ObjectParameters;
		Ar << ObjectProcessStride;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter DispatchParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FShaderParameter ObjectProcessStride;
};

IMPLEMENT_SHADER_TYPE(,FSetupLightVPLsIndirectArgumentsCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("SetupLightVPLsIndirectArgumentsCS"),SF_Compute);

class FLightVPLsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLightVPLsCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLightTileIntersectionParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("LIGHT_VPLS_THREADGROUP_SIZE"), LightVPLsThreadGroupSize);
	}

	/** Default constructor. */
	FLightVPLsCS() {}

	/** Initialization constructor. */
	FLightVPLsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		LightSourceRadius.Bind(Initializer.ParameterMap, TEXT("LightSourceRadius"));
		LightPositionAndInvRadius.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		TanLightAngleAndNormalThreshold.Bind(Initializer.ParameterMap, TEXT("TanLightAngleAndNormalThreshold"));
		LightColor.Bind(Initializer.ParameterMap, TEXT("LightColor"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		SurfelParameters.Bind(Initializer.ParameterMap);
		LightTileIntersectionParameters.Bind(Initializer.ParameterMap);
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		ShadowObjectIndirectArguments.Bind(Initializer.ParameterMap, TEXT("ShadowObjectIndirectArguments"));
		ShadowCulledObjectBounds.Bind(Initializer.ParameterMap, TEXT("ShadowCulledObjectBounds"));
		ShadowCulledObjectData.Bind(Initializer.ParameterMap, TEXT("ShadowCulledObjectData"));
		ObjectProcessStride.Bind(Initializer.ParameterMap, TEXT("ObjectProcessStride"));
		ObjectProcessStartIndex.Bind(Initializer.ParameterMap, TEXT("ObjectProcessStartIndex"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FLightSceneProxy* LightSceneProxy,
		const FMatrix& WorldToShadowMatrixValue,
		const FDistanceFieldAOParameters& Parameters,
		FLightTileIntersectionResources* TileIntersectionResources)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		const FScene* Scene = (const FScene*)View.Family->Scene;

		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		SurfelParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.SurfelBuffers, *Scene->DistanceFieldSceneData.InstancedSurfelBuffers);

		FLightParameters LightParameters;

		LightSceneProxy->GetParameters(LightParameters);

		SetShaderValue(RHICmdList, ShaderRHI, LightDirection, LightParameters.NormalizedLightDirection);
		SetShaderValue(RHICmdList, ShaderRHI, LightPositionAndInvRadius, LightParameters.LightPositionAndInvRadius);
		// Default light source radius of 0 gives poor results
		SetShaderValue(RHICmdList, ShaderRHI, LightSourceRadius, LightParameters.LightSourceRadius == 0 ? 20 : FMath::Clamp(LightParameters.LightSourceRadius, .001f, 1.0f / (4 * LightParameters.LightPositionAndInvRadius.W)));

		const float LightSourceAngle = FMath::Clamp(LightSceneProxy->GetLightSourceAngle(), 0.001f, 5.0f) * PI / 180.0f;
		const FVector2D TanLightAngleAndNormalThresholdValue(FMath::Tan(LightSourceAngle), FMath::Cos(PI / 2 + LightSourceAngle));
		SetShaderValue(RHICmdList, ShaderRHI, TanLightAngleAndNormalThreshold, TanLightAngleAndNormalThresholdValue);
		SetShaderValue(RHICmdList, ShaderRHI, LightColor, LightSceneProxy->GetColor() * LightSceneProxy->GetIndirectLightingScale());

		check(TileIntersectionResources || !LightTileIntersectionParameters.IsBound());

		if (TileIntersectionResources)
		{
			LightTileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);
		}

		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);

		SetSRVParameter(RHICmdList, ShaderRHI, ShadowObjectIndirectArguments, GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ShadowCulledObjectBounds, GShadowCulledObjectBuffers.Buffers.Bounds.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ShadowCulledObjectData, GShadowCulledObjectBuffers.Buffers.Data.SRV);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectProcessStride, GVPLSpreadUpdateOver);
		SetShaderValue(RHICmdList, ShaderRHI, ObjectProcessStartIndex, GFrameNumberRenderThread % GVPLSpreadUpdateOver);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		SurfelParameters.UnsetParameters(RHICmdList, GetComputeShader());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << LightDirection;
		Ar << LightPositionAndInvRadius;
		Ar << LightSourceRadius;
		Ar << TanLightAngleAndNormalThreshold;
		Ar << LightColor;
		Ar << ObjectParameters;
		Ar << SurfelParameters;
		Ar << LightTileIntersectionParameters;
		Ar << WorldToShadow;
		Ar << ShadowObjectIndirectArguments;
		Ar << ShadowCulledObjectBounds;
		Ar << ShadowCulledObjectData;
		Ar << ObjectProcessStride;
		Ar << ObjectProcessStartIndex;
		return bShaderHasOutdatedParameters;
	}

private:

	FAOParameters AOParameters;
	FShaderParameter LightDirection;
	FShaderParameter LightPositionAndInvRadius;
	FShaderParameter LightSourceRadius;
	FShaderParameter TanLightAngleAndNormalThreshold;
	FShaderParameter LightColor;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FSurfelBufferParameters SurfelParameters;
	FLightTileIntersectionParameters LightTileIntersectionParameters;
	FShaderParameter WorldToShadow;
	FShaderResourceParameter ShadowObjectIndirectArguments;
	FShaderResourceParameter ShadowCulledObjectBounds;
	FShaderResourceParameter ShadowCulledObjectData;
	FShaderParameter ObjectProcessStride;
	FShaderParameter ObjectProcessStartIndex;
};

IMPLEMENT_SHADER_TYPE(,FLightVPLsCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("LightVPLsCS"),SF_Compute);

void UpdateVPLs(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FScene* Scene,
	const FDistanceFieldAOParameters& Parameters)
{
	if (GVPLMeshGlobalIllumination)
	{
		if (GVPLSurfelRepresentation)
		{
			SCOPED_DRAW_EVENT(RHICmdList, UpdateVPLs);

			const FLightSceneProxy* DirectionalLightProxy = NULL;

			for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
			{
				const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
				const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

				if (LightSceneInfo->ShouldRenderLightViewIndependent() 
					&& LightSceneInfo->Proxy->GetLightType() == LightType_Directional
					&& LightSceneInfo->Proxy->CastsDynamicShadow())
				{
					DirectionalLightProxy = LightSceneInfo->Proxy;
					break;
				}
			}

			FMatrix DirectionalLightWorldToShadow = FMatrix::Identity;

			if (DirectionalLightProxy)
			{
				{
					int32 NumPlanes = 0;
					const FPlane* PlaneData = NULL;
					FVector4 ShadowBoundingSphereValue(0, 0, 0, 0);
					FShadowCascadeSettings CascadeSettings;
					FSphere ShadowBounds;
					FConvexVolume FrustumVolume;

					{
						const float ConeExpandDistance = Parameters.ObjectMaxOcclusionDistance;
						const float TanHalfFOV = 1.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0];
						const float VertexPullbackLength = ConeExpandDistance / TanHalfFOV;

						// Pull back cone vertex to contain VPLs outside of the view
						const FVector ViewConeVertex = View.ViewMatrices.GetViewOrigin() - View.GetViewDirection() * VertexPullbackLength;

						//@todo - expand by AOObjectMaxDistance
						ShadowBounds = DirectionalLightProxy->GetShadowSplitBoundsDepthRange(
							View, 
							ViewConeVertex,
							View.NearClippingDistance, 
							GetMaxAOViewDistance() + VertexPullbackLength + Parameters.ObjectMaxOcclusionDistance, 
							&CascadeSettings); 

						FSphere SubjectBounds(FVector::ZeroVector, ShadowBounds.W);

						const FMatrix& WorldToLight = DirectionalLightProxy->GetWorldToLight();
						const FMatrix InitializerWorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
						const FVector InitializerFaceDirection = FVector(1,0,0);

						FVector	XAxis, YAxis;
						InitializerFaceDirection.FindBestAxisVectors(XAxis, YAxis);
						const FMatrix WorldToLightScaled = InitializerWorldToLight * FScaleMatrix(FVector(1.0f,1.0f / SubjectBounds.W,1.0f / SubjectBounds.W));
						const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,InitializerFaceDirection.GetSafeNormal(),FVector::ZeroVector);

						NumPlanes = CascadeSettings.ShadowBoundsAccurate.Planes.Num();
						PlaneData = CascadeSettings.ShadowBoundsAccurate.Planes.GetData();

						DirectionalLightWorldToShadow = FTranslationMatrix(-ShadowBounds.Center) 
							* WorldToFace 
							* FShadowProjectionMatrix(-GVPLDirectionalLightTraceDistance / 2, GVPLDirectionalLightTraceDistance / 2, FVector4(0,0,0,1));
					}

					CullDistanceFieldObjectsForLight(
						RHICmdList,
						View,
						DirectionalLightProxy, 
						DirectionalLightWorldToShadow,
						NumPlanes,
						PlaneData,
						ShadowBoundingSphereValue,
						ShadowBounds.W,
						GVPLPlacementTileIntersectionResources);
				}

				SCOPED_DRAW_EVENT(RHICmdList, LightVPLs);

				{
					TShaderMapRef<FSetupLightVPLsIndirectArgumentsCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View);

					DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
					ComputeShader->UnsetParameters(RHICmdList);
				}

				{
					TShaderMapRef<FLightVPLsCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, DirectionalLightProxy, DirectionalLightWorldToShadow, Parameters, GVPLPlacementTileIntersectionResources.Get());
					DispatchIndirectComputeShader(RHICmdList, *ComputeShader, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch.Buffer, 0);
					ComputeShader->UnsetParameters(RHICmdList);
				}
			}
			else
			{
				ClearUAV(RHICmdList, Scene->DistanceFieldSceneData.InstancedSurfelBuffers->VPLFlux, 0);
			}
		}
		else
		{
			PlaceVPLs(RHICmdList, View, Scene, Parameters);
		}
	}
}

const int32 GScreenGridIrradianceThreadGroupSizeX = 8;

class FComputeStepBentNormalScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeStepBentNormalScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SCREEN_GRID_IRRADIANCE_THREADGROUP_SIZE_X"), GScreenGridIrradianceThreadGroupSizeX);
		extern int32 GConeTraceDownsampleFactor;
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FComputeStepBentNormalScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		ConeDepthVisibilityFunction.Bind(Initializer.ParameterMap, TEXT("ConeDepthVisibilityFunction"));
		StepBentNormal.Bind(Initializer.ParameterMap, TEXT("StepBentNormal"));
	}

	FComputeStepBentNormalScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FAOScreenGridResources& ScreenGridResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(View.Family->FrameNumber, SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);

		SetSRVParameter(RHICmdList, ShaderRHI, ConeDepthVisibilityFunction, ScreenGridResources.ConeDepthVisibilityFunction.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.StepBentNormal.UAV);
		StepBentNormal.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources.StepBentNormal);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FAOScreenGridResources& ScreenGridResources)
	{
		StepBentNormal.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.StepBentNormal.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ScreenGridParameters;
		Ar << BentNormalNormalizeFactor;
		Ar << ConeDepthVisibilityFunction;
		Ar << StepBentNormal;
		return bShaderHasOutdatedParameters;
	}

private:

	FScreenGridParameters ScreenGridParameters;
	FShaderParameter BentNormalNormalizeFactor;
	FShaderResourceParameter ConeDepthVisibilityFunction;
	FRWShaderParameter StepBentNormal;
};

IMPLEMENT_SHADER_TYPE(,FComputeStepBentNormalScreenGridCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("ComputeStepBentNormalScreenGridCS"),SF_Compute);


class FComputeIrradianceScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeIrradianceScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CULLED_TILE_SIZEX"), GDistanceFieldAOTileSizeX);
		extern int32 GConeTraceDownsampleFactor;
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);
		OutEnvironment.SetDefine(TEXT("IRRADIANCE_FROM_SURFELS"), TEXT("1"));

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FComputeIrradianceScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		SurfelParameters.Bind(Initializer.ParameterMap);
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		TileArrayData.Bind(Initializer.ParameterMap, TEXT("TileArrayData"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		TileListGroupSize.Bind(Initializer.ParameterMap, TEXT("TileListGroupSize"));
		VPLGatherRadius.Bind(Initializer.ParameterMap, TEXT("VPLGatherRadius"));
		StepBentNormalBuffer.Bind(Initializer.ParameterMap, TEXT("StepBentNormalBuffer"));
		SurfelIrradiance.Bind(Initializer.ParameterMap, TEXT("SurfelIrradiance"));
	}

	FComputeIrradianceScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		extern TGlobalResource<FDistanceFieldObjectBufferResource> GAOCulledObjectBuffers;
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		SurfelParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.SurfelBuffers, *Scene->DistanceFieldSceneData.InstancedSurfelBuffers);

		FTileIntersectionResources* TileIntersectionResources = View.ViewState->AOTileIntersectionResources;

		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);
		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileIntersectionResources->TileDimensions);

		SetShaderValue(RHICmdList, ShaderRHI, VPLGatherRadius, Parameters.ObjectMaxOcclusionDistance);

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;

		SetSRVParameter(RHICmdList, ShaderRHI, StepBentNormalBuffer, ScreenGridResources->StepBentNormal.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources->SurfelIrradiance.UAV);
		SurfelIrradiance.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources->SurfelIrradiance);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		SurfelIrradiance.UnsetUAV(RHICmdList, GetComputeShader());

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources->SurfelIrradiance.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << ScreenGridParameters;
		Ar << SurfelParameters;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileConeDepthRanges;
		Ar << TileListGroupSize;
		Ar << VPLGatherRadius;
		Ar << StepBentNormalBuffer;
		Ar << SurfelIrradiance;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FScreenGridParameters ScreenGridParameters;
	FSurfelBufferParameters SurfelParameters;
	FShaderResourceParameter TileHeadDataUnpacked;
	FShaderResourceParameter TileArrayData;
	FShaderResourceParameter TileConeDepthRanges;
	FShaderParameter TileListGroupSize;
	FShaderParameter VPLGatherRadius;
	FShaderResourceParameter StepBentNormalBuffer;
	FRWShaderParameter SurfelIrradiance;
};

IMPLEMENT_SHADER_TYPE(,FComputeIrradianceScreenGridCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("ComputeIrradianceScreenGridCS"),SF_Compute);


class FCombineIrradianceScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCombineIrradianceScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SCREEN_GRID_IRRADIANCE_THREADGROUP_SIZE_X"), GScreenGridIrradianceThreadGroupSizeX);
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FCombineIrradianceScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		SurfelIrradiance.Bind(Initializer.ParameterMap, TEXT("SurfelIrradiance"));
		HeightfieldIrradiance.Bind(Initializer.ParameterMap, TEXT("HeightfieldIrradiance"));
		ScreenGridConeVisibilitySize.Bind(Initializer.ParameterMap, TEXT("ScreenGridConeVisibilitySize"));
	}

	FCombineIrradianceScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FAOScreenGridResources& ScreenGridResources,
		FSceneRenderTargetItem& IrradianceTextureValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetSRVParameter(RHICmdList, ShaderRHI, SurfelIrradiance, ScreenGridResources.SurfelIrradiance.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, HeightfieldIrradiance, ScreenGridResources.HeightfieldIrradiance.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, IrradianceTextureValue.UAV);
		IrradianceTexture.SetTexture(RHICmdList, ShaderRHI, IrradianceTextureValue.ShaderResourceTexture, IrradianceTextureValue.UAV);

		SetShaderValue(RHICmdList, ShaderRHI, ScreenGridConeVisibilitySize, ScreenGridResources.ScreenGridDimensions);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& IrradianceTextureValue)
	{
		IrradianceTexture.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, IrradianceTextureValue.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IrradianceTexture;
		Ar << SurfelIrradiance;
		Ar << HeightfieldIrradiance;
		Ar << ScreenGridConeVisibilitySize;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter IrradianceTexture;
	FShaderResourceParameter SurfelIrradiance;
	FShaderResourceParameter HeightfieldIrradiance;
	FShaderParameter ScreenGridConeVisibilitySize;
};

IMPLEMENT_SHADER_TYPE(,FCombineIrradianceScreenGridCS,TEXT("/Engine/Private/DistanceFieldGlobalIllumination.usf"),TEXT("CombineIrradianceScreenGridCS"),SF_Compute);


void ComputeIrradianceForScreenGrid(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FScene* Scene,
	const FDistanceFieldAOParameters& Parameters,
	FSceneRenderTargetItem& DistanceFieldNormal, 
	const FAOScreenGridResources& ScreenGridResources,
	FSceneRenderTargetItem& IrradianceTexture)
{
	const uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GScreenGridIrradianceThreadGroupSizeX);
	const uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GScreenGridIrradianceThreadGroupSizeX);

	ClearUAV(RHICmdList, ScreenGridResources.HeightfieldIrradiance, 0);
	ClearUAV(RHICmdList, ScreenGridResources.SurfelIrradiance, 0);

	View.HeightfieldLightingViewInfo.
		ComputeIrradianceForScreenGrid(View, RHICmdList, DistanceFieldNormal, ScreenGridResources, Parameters);
	
	if (GVPLMeshGlobalIllumination)
	{
		{
			SCOPED_DRAW_EVENT(RHICmdList, ComputeStepBentNormal);

			TShaderMapRef<FComputeStepBentNormalScreenGridCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, ScreenGridResources);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, ScreenGridResources);
		}

		if (GVPLSurfelRepresentation)
		{
			SCOPED_DRAW_EVENT(RHICmdList, MeshIrradiance);

			TShaderMapRef<FComputeIrradianceScreenGridCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, Parameters);

			const uint32 ComputeIrradianceGroupSizeX = View.ViewRect.Size().X / GAODownsampleFactor;
			const uint32 ComputeIrradianceGroupSizeY = View.ViewRect.Size().Y / GAODownsampleFactor;
			DispatchComputeShader(RHICmdList, *ComputeShader, ComputeIrradianceGroupSizeX, ComputeIrradianceGroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, View);
		}
	}
		
	{
		TShaderMapRef<FCombineIrradianceScreenGridCS> ComputeShader(View.ShaderMap);
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		ComputeShader->SetParameters(RHICmdList, View, ScreenGridResources, IrradianceTexture);
		DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
		ComputeShader->UnsetParameters(RHICmdList, IrradianceTexture);
	}
}


void ListDistanceFieldGIMemory(const FViewInfo& View)
{
	const FScene* Scene = (const FScene*)View.Family->Scene;

	if (GVPLPlacementTileIntersectionResources)
	{
		UE_LOG(LogTemp, Log, TEXT("   Shadow tile culled objects %.3fMb"), GVPLPlacementTileIntersectionResources->GetSizeBytes() / 1024.0f / 1024.0f);
	}
}
