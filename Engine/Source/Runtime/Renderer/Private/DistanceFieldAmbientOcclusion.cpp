// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldAmbientOcclusion.cpp
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
#include "DistanceFieldGlobalIllumination.h"
#include "RendererModule.h"
#include "PipelineStateCache.h"

int32 GDistanceFieldAO = 1;
FAutoConsoleVariableRef CVarDistanceFieldAO(
	TEXT("r.DistanceFieldAO"),
	GDistanceFieldAO,
	TEXT("Whether the distance field AO feature is allowed, which is used to implement shadows of Movable sky lights from static meshes."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GDistanceFieldAOApplyToStaticIndirect = 0;
FAutoConsoleVariableRef CVarDistanceFieldAOApplyToStaticIndirect(
	TEXT("r.AOApplyToStaticIndirect"),
	GDistanceFieldAOApplyToStaticIndirect,
	TEXT("Whether to apply DFAO as indirect shadowing even to static indirect sources (lightmaps + stationary skylight + reflection captures)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GDistanceFieldAOSpecularOcclusionMode = 1;
FAutoConsoleVariableRef CVarDistanceFieldAOSpecularOcclusionMode(
	TEXT("r.AOSpecularOcclusionMode"),
	GDistanceFieldAOSpecularOcclusionMode,
	TEXT("Determines how specular should be occluded by DFAO\n")
	TEXT("0: Apply non-directional AO to specular.\n")
	TEXT("1: (default) Intersect the reflection cone with the unoccluded cone produced by DFAO.  This gives more accurate occlusion than 0, but can bring out DFAO sampling artifacts.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

bool IsDistanceFieldGIAllowed(const FViewInfo& View)
{
	return DoesPlatformSupportDistanceFieldGI(View.GetShaderPlatform())
		&& (View.Family->EngineShowFlags.VisualizeDistanceFieldGI || (View.Family->EngineShowFlags.DistanceFieldGI && GDistanceFieldGI && View.Family->EngineShowFlags.GlobalIllumination));
}

float GAOStepExponentScale = .5f;
FAutoConsoleVariableRef CVarAOStepExponentScale(
	TEXT("r.AOStepExponentScale"),
	GAOStepExponentScale,
	TEXT("Exponent used to distribute AO samples along a cone direction."),
	ECVF_RenderThreadSafe
	);

float GAOMaxViewDistance = 20000;
FAutoConsoleVariableRef CVarAOMaxViewDistance(
	TEXT("r.AOMaxViewDistance"),
	GAOMaxViewDistance,
	TEXT("The maximum distance that AO will be computed at."),
	ECVF_RenderThreadSafe
	);

int32 GAOComputeShaderNormalCalculation = 0;
FAutoConsoleVariableRef CVarAOComputeShaderNormalCalculation(
	TEXT("r.AOComputeShaderNormalCalculation"),
	GAOComputeShaderNormalCalculation,
	TEXT("Whether to use the compute shader version of the distance field normal computation."),
	ECVF_RenderThreadSafe
	);

int32 GAOSampleSet = 1;
FAutoConsoleVariableRef CVarAOSampleSet(
	TEXT("r.AOSampleSet"),
	GAOSampleSet,
	TEXT("0 = Original set, 1 = Relaxed set"),
	ECVF_RenderThreadSafe
	);

int32 GAOOverwriteSceneColor = 0;
FAutoConsoleVariableRef CVarAOOverwriteSceneColor(
	TEXT("r.AOOverwriteSceneColor"),
	GAOOverwriteSceneColor,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GAOJitterConeDirections = 0;
FAutoConsoleVariableRef CVarAOJitterConeDirections(
	TEXT("r.AOJitterConeDirections"),
	GAOJitterConeDirections,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GMaxDistanceFieldObjectsPerCullTile = 512;
FAutoConsoleVariableRef CVarMaxDistanceFieldObjectsPerCullTile(
	TEXT("r.AOMaxObjectsPerCullTile"),
	GMaxDistanceFieldObjectsPerCullTile,
	TEXT("Determines how much memory should be allocated in distance field object culling data structures.  Too much = memory waste, too little = flickering due to buffer overflow."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

TGlobalResource<FTemporaryIrradianceCacheResources> GTemporaryIrradianceCacheResources;

int32 GDistanceFieldAOTileSizeX = 16;
int32 GDistanceFieldAOTileSizeY = 16;

DEFINE_LOG_CATEGORY(LogDistanceField);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FAOSampleData2,TEXT("AOSamples2"));

FDistanceFieldAOParameters::FDistanceFieldAOParameters(float InOcclusionMaxDistance, float InContrast)
{
	Contrast = FMath::Clamp(InContrast, .01f, 2.0f);
	InOcclusionMaxDistance = FMath::Clamp(InOcclusionMaxDistance, 2.0f, 3000.0f);

	if (GAOGlobalDistanceField != 0)
	{
		extern float GAOGlobalDFStartDistance;
		ObjectMaxOcclusionDistance = FMath::Min(InOcclusionMaxDistance, GAOGlobalDFStartDistance);
		GlobalMaxOcclusionDistance = InOcclusionMaxDistance >= GAOGlobalDFStartDistance ? InOcclusionMaxDistance : 0;
	}
	else
	{
		ObjectMaxOcclusionDistance = InOcclusionMaxDistance;
		GlobalMaxOcclusionDistance = 0;
	}
}

FIntPoint GetBufferSizeForAO()
{
	return FIntPoint::DivideAndRoundDown(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), GAODownsampleFactor);
}

// Sample set restricted to not self-intersect a surface based on cone angle .475882232
// Coverage of hemisphere = 0.755312979
const FVector SpacedVectors9[] = 
{
	FVector(-0.573257625, 0.625250816, 0.529563010),
	FVector(0.253354192, -0.840093017, 0.479640961),
	FVector(-0.421664953, -0.718063235, 0.553700149),
	FVector(0.249163717, 0.796005428, 0.551627457),
	FVector(0.375082791, 0.295851320, 0.878512800),
	FVector(-0.217619032, 0.00193520682, 0.976031899),
	FVector(-0.852834642, 0.0111727007, 0.522061586),
	FVector(0.745701790, 0.239393353, 0.621787369),
	FVector(-0.151036426, -0.465937436, 0.871831656)
};

// Generated from SpacedVectors9 by applying repulsion forces until convergence
const FVector RelaxedSpacedVectors9[] = 
{
	FVector(-0.467612, 0.739424, 0.484347),
	FVector(0.517459, -0.705440, 0.484346),
	FVector(-0.419848, -0.767551, 0.484347),
	FVector(0.343077, 0.804802, 0.484347),
	FVector(0.364239, 0.244290, 0.898695),
	FVector(-0.381547, 0.185815, 0.905481),
	FVector(-0.870176, -0.090559, 0.484347),
	FVector(0.874448, 0.027390, 0.484346),
	FVector(0.032967, -0.435625, 0.899524)
};

float TemporalHalton2( int32 Index, int32 Base )
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

void GetSpacedVectors(uint32 FrameNumber, TArray<FVector, TInlineAllocator<9> >& OutVectors)
{
	OutVectors.Empty(ARRAY_COUNT(SpacedVectors9));

	if (GAOSampleSet == 0)
	{
		for (int32 i = 0; i < ARRAY_COUNT(SpacedVectors9); i++)
		{
			OutVectors.Add(SpacedVectors9[i]);
		}
	}
	else
	{
		for (int32 i = 0; i < ARRAY_COUNT(RelaxedSpacedVectors9); i++)
		{
			OutVectors.Add(RelaxedSpacedVectors9[i]);
		}
	}

	if (GAOJitterConeDirections)
	{
		float RandomAngle = TemporalHalton2(FrameNumber & 1023, 2) * 2 * PI;
		float CosRandomAngle = FMath::Cos(RandomAngle);
		float SinRandomAngle = FMath::Sin(RandomAngle);

		for (int32 i = 0; i < OutVectors.Num(); i++)
		{
			FVector ConeDirection = OutVectors[i];
			FVector2D ConeDirectionXY(ConeDirection.X, ConeDirection.Y);
			ConeDirectionXY = FVector2D(FVector2D::DotProduct(ConeDirectionXY, FVector2D(CosRandomAngle, -SinRandomAngle)), FVector2D::DotProduct(ConeDirectionXY, FVector2D(SinRandomAngle, CosRandomAngle)));
			OutVectors[i].X = ConeDirectionXY.X;
			OutVectors[i].Y = ConeDirectionXY.Y;
		}
	}
}

// Cone half angle derived from each cone covering an equal solid angle
float GAOConeHalfAngle = FMath::Acos(1 - 1.0f / (float)ARRAY_COUNT(SpacedVectors9));

// Number of AO sample positions along each cone
// Must match shader code
uint32 GAONumConeSteps = 10;

bool bListMemoryNextFrame = false;

void OnListMemory(UWorld* InWorld)
{
	bListMemoryNextFrame = true;
}

FAutoConsoleCommandWithWorld ListMemoryConsoleCommand(
	TEXT("r.AOListMemory"),
	TEXT(""),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnListMemory)
	);

bool bListMeshDistanceFieldsMemoryNextFrame = false;

void OnListMeshDistanceFields(UWorld* InWorld)
{
	bListMeshDistanceFieldsMemoryNextFrame = true;
}

FAutoConsoleCommandWithWorld ListMeshDistanceFieldsMemoryConsoleCommand(
	TEXT("r.AOListMeshDistanceFields"),
	TEXT(""),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnListMeshDistanceFields)
	);

class FComputeDistanceFieldNormalPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeDistanceFieldNormalPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	/** Default constructor. */
	FComputeDistanceFieldNormalPS() {}

	/** Initialization constructor. */
	FComputeDistanceFieldNormalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << AOParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
};

IMPLEMENT_SHADER_TYPE(,FComputeDistanceFieldNormalPS,TEXT("/Engine/Private/DistanceFieldScreenGridLighting.usf"),TEXT("ComputeDistanceFieldNormalPS"),SF_Pixel);


class FComputeDistanceFieldNormalCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeDistanceFieldNormalCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	/** Default constructor. */
	FComputeDistanceFieldNormalCS() {}

	/** Initialization constructor. */
	FComputeDistanceFieldNormalCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DistanceFieldNormal.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormal"));
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldNormalValue, const FDistanceFieldAOParameters& Parameters)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DistanceFieldNormalValue.UAV);
		DistanceFieldNormal.SetTexture(RHICmdList, ShaderRHI, DistanceFieldNormalValue.ShaderResourceTexture, DistanceFieldNormalValue.UAV);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& DistanceFieldNormalValue)
	{
		DistanceFieldNormal.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, DistanceFieldNormalValue.UAV);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DistanceFieldNormal;
		Ar << DeferredParameters;
		Ar << AOParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter DistanceFieldNormal;
	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
};

IMPLEMENT_SHADER_TYPE(,FComputeDistanceFieldNormalCS,TEXT("/Engine/Private/DistanceFieldScreenGridLighting.usf"),TEXT("ComputeDistanceFieldNormalCS"),SF_Compute);

void ComputeDistanceFieldNormal(FRHICommandListImmediate& RHICmdList, const TArray<FViewInfo>& Views, FSceneRenderTargetItem& DistanceFieldNormal, const FDistanceFieldAOParameters& Parameters)
{
	if (GAOComputeShaderNormalCalculation)
	{
		SetRenderTarget(RHICmdList, NULL, NULL);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX);
			uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY);

			{
				SCOPED_DRAW_EVENT(RHICmdList, ComputeNormalCS);
				TShaderMapRef<FComputeDistanceFieldNormalCS> ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, Parameters);
				DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

				ComputeShader->UnsetParameters(RHICmdList, DistanceFieldNormal);
			}
		}
	}
	else
	{
		SetRenderTarget(RHICmdList, DistanceFieldNormal.TargetableTexture, NULL, true);
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			SCOPED_DRAW_EVENT(RHICmdList, ComputeNormal);

			RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FComputeDistanceFieldNormalPS> PixelShader(View.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters(RHICmdList, View, Parameters);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
				FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
				*VertexShader);
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, DistanceFieldNormal.TargetableTexture);
	}
}

/** Generates a pseudo-random position inside the unit sphere, uniformly distributed over the volume of the sphere. */
FVector GetUnitPosition2(FRandomStream& RandomStream)
{
	FVector Result;
	// Use rejection sampling to generate a valid sample
	do
	{
		Result.X = RandomStream.GetFraction() * 2 - 1;
		Result.Y = RandomStream.GetFraction() * 2 - 1;
		Result.Z = RandomStream.GetFraction() * 2 - 1;
	} while( Result.SizeSquared() > 1.f );
	return Result;
}

/** Generates a pseudo-random unit vector, uniformly distributed over all directions. */
FVector GetUnitVector2(FRandomStream& RandomStream)
{
	return GetUnitPosition2(RandomStream).GetUnsafeNormal();
}

void GenerateBestSpacedVectors()
{
	static bool bGenerated = false;
	bool bApplyRepulsion = false;

	if (bApplyRepulsion && !bGenerated)
	{
		bGenerated = true;

		FVector OriginalSpacedVectors9[ARRAY_COUNT(SpacedVectors9)];

		for (int32 i = 0; i < ARRAY_COUNT(OriginalSpacedVectors9); i++)
		{
			OriginalSpacedVectors9[i] = SpacedVectors9[i];
		}

		float CosHalfAngle = 1 - 1.0f / (float)ARRAY_COUNT(OriginalSpacedVectors9);
		// Used to prevent self-shadowing on a plane
		float AngleBias = .03f;
		float MinAngle = FMath::Acos(CosHalfAngle) + AngleBias;
		float MinZ = FMath::Sin(MinAngle);

		// Relaxation iterations by repulsion
		for (int32 Iteration = 0; Iteration < 10000; Iteration++)
		{
			for (int32 i = 0; i < ARRAY_COUNT(OriginalSpacedVectors9); i++)
			{
				FVector Force(0.0f, 0.0f, 0.0f);

				for (int32 j = 0; j < ARRAY_COUNT(OriginalSpacedVectors9); j++)
				{
					if (i != j)
					{
						FVector Distance = OriginalSpacedVectors9[i] - OriginalSpacedVectors9[j];
						float Dot = OriginalSpacedVectors9[i] | OriginalSpacedVectors9[j];

						if (Dot > 0)
						{
							// Repulsion force
							Force += .001f * Distance.GetSafeNormal() * Dot * Dot * Dot * Dot;
						}
					}
				}

				FVector NewPosition = OriginalSpacedVectors9[i] + Force;
				NewPosition.Z = FMath::Max(NewPosition.Z, MinZ);
				NewPosition = NewPosition.GetSafeNormal();
				OriginalSpacedVectors9[i] = NewPosition;
			}
		}

		for (int32 i = 0; i < ARRAY_COUNT(OriginalSpacedVectors9); i++)
		{
			UE_LOG(LogDistanceField, Log, TEXT("FVector(%f, %f, %f),"), OriginalSpacedVectors9[i].X, OriginalSpacedVectors9[i].Y, OriginalSpacedVectors9[i].Z);
		}

		int32 temp = 0;
	}

	bool bBruteForceGenerateConeDirections = false;

	if (bBruteForceGenerateConeDirections)
	{
		FVector BestSpacedVectors9[9];
		float BestCoverage = 0;
		// Each cone covers an area of ConeSolidAngle = HemisphereSolidAngle / NumCones
		// HemisphereSolidAngle = 2 * PI
		// ConeSolidAngle = 2 * PI * (1 - cos(ConeHalfAngle))
		// cos(ConeHalfAngle) = 1 - 1 / NumCones
		float CosHalfAngle = 1 - 1.0f / (float)ARRAY_COUNT(BestSpacedVectors9);
		// Prevent self-intersection in sample set
		float MinAngle = FMath::Acos(CosHalfAngle);
		float MinZ = FMath::Sin(MinAngle);
		FRandomStream RandomStream(123567);

		// Super slow random brute force search
		for (int i = 0; i < 1000000; i++)
		{
			FVector CandidateSpacedVectors[ARRAY_COUNT(BestSpacedVectors9)];

			for (int j = 0; j < ARRAY_COUNT(CandidateSpacedVectors); j++)
			{
				FVector NewSample;

				// Reject invalid directions until we get a valid one
				do 
				{
					NewSample = GetUnitVector2(RandomStream);
				} 
				while (NewSample.Z <= MinZ);

				CandidateSpacedVectors[j] = NewSample;
			}

			float Coverage = 0;
			int NumSamples = 10000;

			// Determine total cone coverage with monte carlo estimation
			for (int sample = 0; sample < NumSamples; sample++)
			{
				FVector NewSample;

				do 
				{
					NewSample = GetUnitVector2(RandomStream);
				} 
				while (NewSample.Z <= 0);

				bool bIntersects = false;

				for (int j = 0; j < ARRAY_COUNT(CandidateSpacedVectors); j++)
				{
					if (FVector::DotProduct(CandidateSpacedVectors[j], NewSample) > CosHalfAngle)
					{
						bIntersects = true;
						break;
					}
				}

				Coverage += bIntersects ? 1 / (float)NumSamples : 0;
			}

			if (Coverage > BestCoverage)
			{
				BestCoverage = Coverage;

				for (int j = 0; j < ARRAY_COUNT(CandidateSpacedVectors); j++)
				{
					BestSpacedVectors9[j] = CandidateSpacedVectors[j];
				}
			}
		}

		int32 temp = 0;
	}
}

void ListDistanceFieldLightingMemory(const FViewInfo& View, FSceneRenderer& SceneRenderer)
{
	const FScene* Scene = (const FScene*)View.Family->Scene;
	UE_LOG(LogRenderer, Log, TEXT("Shared GPU memory (excluding render targets)"));

	if (Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0)
	{
		UE_LOG(LogRenderer, Log, TEXT("   Scene Object data %.3fMb"), Scene->DistanceFieldSceneData.ObjectBuffers->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	UE_LOG(LogRenderer, Log, TEXT("   %s"), *GDistanceFieldVolumeTextureAtlas.GetSizeString());
	extern FString GetObjectBufferMemoryString();
	UE_LOG(LogRenderer, Log, TEXT("   %s"), *GetObjectBufferMemoryString());
	UE_LOG(LogRenderer, Log, TEXT(""));
	UE_LOG(LogRenderer, Log, TEXT("Distance Field AO"));
	
	UE_LOG(LogRenderer, Log, TEXT("   Temporary cache %.3fMb"), GTemporaryIrradianceCacheResources.GetSizeBytes() / 1024.0f / 1024.0f);
	UE_LOG(LogRenderer, Log, TEXT("   Culled objects %.3fMb"), GAOCulledObjectBuffers.Buffers.GetSizeBytes() / 1024.0f / 1024.0f);

	FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

	if (TileIntersectionResources)
	{
		UE_LOG(LogRenderer, Log, TEXT("   Tile Culled objects %.3fMb"), TileIntersectionResources->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	FAOScreenGridResources* ScreenGridResources = ((FSceneViewState*)View.State)->AOScreenGridResources;

	if (ScreenGridResources)
	{
		UE_LOG(LogRenderer, Log, TEXT("   Screen grid temporaries %.3fMb"), ScreenGridResources->GetSizeBytesForAO() / 1024.0f / 1024.0f);
	}
	
	UE_LOG(LogRenderer, Log, TEXT(""));
	UE_LOG(LogRenderer, Log, TEXT("Ray Traced Distance Field Shadows"));

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		FVisibleLightInfo& VisibleLightInfo = SceneRenderer.VisibleLightInfos[LightSceneInfo->Id];

		for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

			if (ProjectedShadowInfo->bRayTracedDistanceField && LightSceneInfo->TileIntersectionResources)
			{
				UE_LOG(LogRenderer, Log, TEXT("   Light Tile Culled objects %.3fMb"), LightSceneInfo->TileIntersectionResources->GetSizeBytes() / 1024.0f / 1024.0f);
			}
		}
	}

	extern void ListGlobalDistanceFieldMemory();
	ListGlobalDistanceFieldMemory();

	UE_LOG(LogRenderer, Log, TEXT(""));
	UE_LOG(LogRenderer, Log, TEXT("Distance Field GI"));

	if (Scene->DistanceFieldSceneData.SurfelBuffers)
	{
		UE_LOG(LogRenderer, Log, TEXT("   Scene surfel data %.3fMb"), Scene->DistanceFieldSceneData.SurfelBuffers->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	if (Scene->DistanceFieldSceneData.InstancedSurfelBuffers)
	{
		UE_LOG(LogRenderer, Log, TEXT("   Instanced scene surfel data %.3fMb"), Scene->DistanceFieldSceneData.InstancedSurfelBuffers->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	if (ScreenGridResources)
	{
		UE_LOG(LogRenderer, Log, TEXT("   Screen grid temporaries %.3fMb"), ScreenGridResources->GetSizeBytesForGI() / 1024.0f / 1024.0f);
	}

	extern void ListDistanceFieldGIMemory(const FViewInfo& View);
	ListDistanceFieldGIMemory(View);
}

bool SupportsDistanceFieldAO(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return GDistanceFieldAO 
		// Pre-GCN AMD cards have a driver bug that prevents the global distance field from being generated correctly
		// Better to disable entirely than to display garbage
		&& !GRHIDeviceIsAMDPreGCNArchitecture
		// Intel HD 4000 hangs in the RHICreateTexture3D call to allocate the large distance field atlas, and virtually no Intel cards can afford it anyway
		&& !IsRHIDeviceIntel()
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldAO(ShaderPlatform);
}

bool ShouldRenderDeferredDynamicSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& !Scene->SkyLight->bWantsStaticShadowing
		&& !Scene->SkyLight->bHasStaticLighting
		&& ViewFamily.EngineShowFlags.SkyLighting
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4
		&& !IsAnyForwardShadingEnabled(Scene->GetShaderPlatform()) 
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling;
}

bool FDeferredShadingSceneRenderer::ShouldPrepareForDistanceFieldAO() const
{
	return SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform())
		&& ((ShouldRenderDeferredDynamicSkyLight(Scene, ViewFamily) && Scene->SkyLight->bCastShadows && ViewFamily.EngineShowFlags.DistanceFieldAO)
			|| ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields
			|| ViewFamily.EngineShowFlags.VisualizeGlobalDistanceField
			|| ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO
			|| ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI
			|| (GDistanceFieldAOApplyToStaticIndirect && ViewFamily.EngineShowFlags.DistanceFieldAO));
}

bool FDeferredShadingSceneRenderer::ShouldPrepareDistanceFieldScene(
	// NvFlow begin
	bool bCustomShouldPrepare
	// NvFlow end
) const
{
	if (!ensure(Scene != nullptr))
	{
		return false;
	}

	if (IsRHIDeviceIntel())
	{
		// Intel HD 4000 hangs in the RHICreateTexture3D call to allocate the large distance field atlas, and virtually no Intel cards can afford it anyway
		return false;
	}

	bool bShouldPrepareForAO = SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform()) && ShouldPrepareForDistanceFieldAO();
	bool bShouldPrepareGlobalDistanceField = ShouldPrepareGlobalDistanceField(bCustomShouldPrepare);
	bool bShouldPrepareForDFInsetIndirectShadow = ShouldPrepareForDFInsetIndirectShadow();

	// Prepare the distance field scene (object buffers and distance field atlas) if any feature needs it
	return bShouldPrepareGlobalDistanceField || bShouldPrepareForAO || ShouldPrepareForDistanceFieldShadows() || bShouldPrepareForDFInsetIndirectShadow;
}

bool FDeferredShadingSceneRenderer::ShouldPrepareGlobalDistanceField(
	// NvFlow begin
	bool bCustomShouldPrepare
	// NvFlow end
) const
{
	if (!ensure(Scene != nullptr))
	{
		return false;
	}

	bool bShouldPrepareForAO = SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform())
		&& (ShouldPrepareForDistanceFieldAO()
			|| ((Views.Num() > 0) && Views[0].bUsesGlobalDistanceField)
			|| ((Scene->FXSystem != nullptr) && Scene->FXSystem->UsesGlobalDistanceField())
			// NvFlow begin
			|| (bCustomShouldPrepare)
			// NvFlow end
			);

	return bShouldPrepareForAO && UseGlobalDistanceField();
}

void FDeferredShadingSceneRenderer::RenderDFAOAsIndirectShadowing(
	FRHICommandListImmediate& RHICmdList,
	const TRefCountPtr<IPooledRenderTarget>& VelocityTexture, 
	TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	if (GDistanceFieldAOApplyToStaticIndirect && ShouldRenderDistanceFieldAO())
	{
		// Use the skylight's max distance if there is one, to be consistent with DFAO shadowing on the skylight
		const float OcclusionMaxDistance = Scene->SkyLight && !Scene->SkyLight->bWantsStaticShadowing ? Scene->SkyLight->OcclusionMaxDistance : Scene->DefaultMaxDistanceFieldOcclusionDistance;
		TRefCountPtr<IPooledRenderTarget> DummyOutput;
		RenderDistanceFieldLighting(RHICmdList, FDistanceFieldAOParameters(OcclusionMaxDistance), VelocityTexture, DynamicBentNormalAO, DummyOutput, true, false, false);
	}
}

bool FDeferredShadingSceneRenderer::RenderDistanceFieldLighting(
	FRHICommandListImmediate& RHICmdList, 
	const FDistanceFieldAOParameters& Parameters, 
	const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
	TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO, 
	TRefCountPtr<IPooledRenderTarget>& OutDynamicIrradiance,
	bool bModulateToSceneColor,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination)
{
	SCOPED_DRAW_EVENT(RHICmdList, RenderDistanceFieldLighting);

	//@todo - support multiple views
	const FViewInfo& View = Views[0];
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (SupportsDistanceFieldAO(View.GetFeatureLevel(), View.GetShaderPlatform())
		&& Views.Num() == 1
		// ViewState is used to cache tile intersection resources which have to be sized based on the view
		&& View.State
		&& View.IsPerspectiveProjection())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderDistanceFieldLighting);

		if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI && Scene->DistanceFieldSceneData.NumObjectsInBuffer)
		{
			check(!Scene->DistanceFieldSceneData.HasPendingOperations());
			const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

			SCOPED_DRAW_EVENT(RHICmdList, DistanceFieldLighting);

			GenerateBestSpacedVectors();

			if (bListMemoryNextFrame)
			{
				bListMemoryNextFrame = false;
				ListDistanceFieldLightingMemory(View, *this);
			}

			if (bListMeshDistanceFieldsMemoryNextFrame)
			{
				bListMeshDistanceFieldsMemoryNextFrame = false;
				GDistanceFieldVolumeTextureAtlas.ListMeshDistanceFields();
			}

			CullObjectsToView(RHICmdList, Scene, View, Parameters, GAOCulledObjectBuffers);

			TRefCountPtr<IPooledRenderTarget> DistanceFieldNormal;

			{
				const FIntPoint BufferSize = GetBufferSizeForAO();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				Desc.Flags |= GFastVRamConfig.DistanceFieldNormal;
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DistanceFieldNormal, TEXT("DistanceFieldNormal"));
			}

			ComputeDistanceFieldNormal(RHICmdList, Views, DistanceFieldNormal->GetRenderTargetItem(), Parameters);

			// Intersect objects with screen tiles, build lists
			FIntPoint TileListGroupSize = BuildTileObjectLists(RHICmdList, Scene, Views, DistanceFieldNormal->GetRenderTargetItem(), Parameters);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, DistanceFieldNormal);

			if (bUseDistanceFieldGI)
			{
				extern void UpdateVPLs(
					FRHICommandListImmediate& RHICmdList,
					const FViewInfo& View,
					const FScene* Scene,
					const FDistanceFieldAOParameters& Parameters);

				UpdateVPLs(RHICmdList, View, Scene, Parameters);
			}

			TRefCountPtr<IPooledRenderTarget> BentNormalOutput;
			TRefCountPtr<IPooledRenderTarget> IrradianceOutput;

			RenderDistanceFieldAOScreenGrid(
				RHICmdList, 
				View,
				TileListGroupSize,
				Parameters, 
				VelocityTexture,
				DistanceFieldNormal, 
				BentNormalOutput, 
				IrradianceOutput);

			if ( IsTransientResourceBufferAliasingEnabled() )
			{
				GAOCulledObjectBuffers.Buffers.DiscardTransientResource();

				FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;
				TileIntersectionResources->DiscardTransientResource();
			}

			RenderCapsuleShadowsForMovableSkylight(RHICmdList, BentNormalOutput);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, BentNormalOutput);

			if (bVisualizeAmbientOcclusion || bVisualizeGlobalIllumination)
			{
				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);
			}
			else
			{
				FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
				Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
				// Make sure we get a signed format
				Desc.Format = PF_FloatRGBA;
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutDynamicBentNormalAO, TEXT("DynamicBentNormalAO"));

				if (bUseDistanceFieldGI)
				{
					Desc.Format = PF_FloatRGB;
					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutDynamicIrradiance, TEXT("DynamicIrradiance"));
				}

				FTextureRHIParamRef RenderTargets[3] =
				{
					OutDynamicBentNormalAO->GetRenderTargetItem().TargetableTexture,
					NULL,
					NULL
				};

				int32 NumRenderTargets = 1;

				if (bModulateToSceneColor)
				{
					RenderTargets[NumRenderTargets] = SceneContext.GetSceneColorSurface();
					NumRenderTargets++;
				}

				if (bUseDistanceFieldGI)
				{
					RenderTargets[NumRenderTargets] = OutDynamicIrradiance->GetRenderTargetItem().TargetableTexture;
					NumRenderTargets++;
				}

				SetRenderTargets(RHICmdList, NumRenderTargets, RenderTargets, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);
			}

			// Upsample to full resolution, write to output
			UpsampleBentNormalAO(RHICmdList, Views, BentNormalOutput, IrradianceOutput, bModulateToSceneColor, bVisualizeAmbientOcclusion, bVisualizeGlobalIllumination);

			if (!bVisualizeAmbientOcclusion && !bVisualizeGlobalIllumination)
			{
				RHICmdList.CopyToResolveTarget(OutDynamicBentNormalAO->GetRenderTargetItem().TargetableTexture, OutDynamicBentNormalAO->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
				if (bUseDistanceFieldGI)
				{
					RHICmdList.CopyToResolveTarget(OutDynamicIrradiance->GetRenderTargetItem().TargetableTexture, OutDynamicIrradiance->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
				}
			}

			return true;
		}
	}

	return false;
}

template<bool bApplyShadowing, bool bSupportIrradiance>
class TDynamicSkyLightDiffusePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDynamicSkyLightDiffusePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("APPLY_SHADOWING"), bApplyShadowing);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
	}

	/** Default constructor. */
	TDynamicSkyLightDiffusePS() {}

	/** Initialization constructor. */
	TDynamicSkyLightDiffusePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		DynamicBentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		DynamicBentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		DynamicIrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		DynamicIrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		ContrastAndNormalizeMulAdd.Bind(Initializer.ParameterMap, TEXT("ContrastAndNormalizeMulAdd"));
		OcclusionExponent.Bind(Initializer.ParameterMap, TEXT("OcclusionExponent"));
		OcclusionTintAndMinOcclusion.Bind(Initializer.ParameterMap, TEXT("OcclusionTintAndMinOcclusion"));
		OcclusionCombineMode.Bind(Initializer.ParameterMap, TEXT("OcclusionCombineMode"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FTextureRHIParamRef DynamicBentNormalAO, IPooledRenderTarget* DynamicIrradiance, const FDistanceFieldAOParameters& Parameters, const FSkyLightSceneProxy* SkyLight)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		SetTextureParameter(RHICmdList, ShaderRHI, DynamicBentNormalAOTexture, DynamicBentNormalAOSampler, TStaticSamplerState<SF_Point>::GetRHI(), DynamicBentNormalAO);

		if (DynamicIrradianceTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, DynamicIrradianceTexture, DynamicIrradianceSampler, TStaticSamplerState<SF_Point>::GetRHI(), DynamicIrradiance->GetRenderTargetItem().ShaderResourceTexture);
		}

		// Scale and bias to remap the contrast curve to [0,1]
		const float Min = 1 / (1 + FMath::Exp(-Parameters.Contrast * (0 * 10 - 5)));
		const float Max = 1 / (1 + FMath::Exp(-Parameters.Contrast * (1 * 10 - 5)));
		const float Mul = 1.0f / (Max - Min);
		const float Add = -Min / (Max - Min);

		SetShaderValue(RHICmdList, ShaderRHI, ContrastAndNormalizeMulAdd, FVector(Parameters.Contrast, Mul, Add));

		SetShaderValue(RHICmdList, ShaderRHI, OcclusionExponent, SkyLight->OcclusionExponent);

		FVector4 OcclusionTintAndMinOcclusionValue = FVector4(SkyLight->OcclusionTint);
		OcclusionTintAndMinOcclusionValue.W = SkyLight->MinOcclusion;
		SetShaderValue(RHICmdList, ShaderRHI, OcclusionTintAndMinOcclusion, OcclusionTintAndMinOcclusionValue);

		SetShaderValue(RHICmdList, ShaderRHI, OcclusionCombineMode, SkyLight->OcclusionCombineMode == OCM_Minimum ? 0.0f : 1.0f);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << DynamicBentNormalAOTexture;
		Ar << DynamicBentNormalAOSampler;
		Ar << DynamicIrradianceTexture;
		Ar << DynamicIrradianceSampler;
		Ar << ContrastAndNormalizeMulAdd;
		Ar << OcclusionExponent;
		Ar << OcclusionTintAndMinOcclusion;
		Ar << OcclusionCombineMode;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter DynamicBentNormalAOTexture;
	FShaderResourceParameter DynamicBentNormalAOSampler;
	FShaderResourceParameter DynamicIrradianceTexture;
	FShaderResourceParameter DynamicIrradianceSampler;
	FShaderParameter ContrastAndNormalizeMulAdd;
	FShaderParameter OcclusionExponent;
	FShaderParameter OcclusionTintAndMinOcclusion;
	FShaderParameter OcclusionCombineMode;
};

#define IMPLEMENT_SKYLIGHT_PS_TYPE(bApplyShadowing, bSupportIrradiance) \
	typedef TDynamicSkyLightDiffusePS<bApplyShadowing, bSupportIrradiance> TDynamicSkyLightDiffusePS##bApplyShadowing##bSupportIrradiance; \
	IMPLEMENT_SHADER_TYPE(template<>,TDynamicSkyLightDiffusePS##bApplyShadowing##bSupportIrradiance,TEXT("/Engine/Private/SkyLighting.usf"),TEXT("SkyLightDiffusePS"),SF_Pixel);

IMPLEMENT_SKYLIGHT_PS_TYPE(true, true)
IMPLEMENT_SKYLIGHT_PS_TYPE(false, true)
IMPLEMENT_SKYLIGHT_PS_TYPE(true, false)
IMPLEMENT_SKYLIGHT_PS_TYPE(false, false)

bool FDeferredShadingSceneRenderer::ShouldRenderDistanceFieldAO() const
{
	return ViewFamily.EngineShowFlags.DistanceFieldAO 
		&& !ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO
		&& !ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI
		&& !ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields
		&& !ViewFamily.EngineShowFlags.VisualizeGlobalDistanceField;
}

void FDeferredShadingSceneRenderer::RenderDynamicSkyLighting(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& VelocityTexture, TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	if (ShouldRenderDeferredDynamicSkyLight(Scene, ViewFamily))
	{
		SCOPED_DRAW_EVENT(RHICmdList, SkyLightDiffuse);

		bool bApplyShadowing = false;

		FDistanceFieldAOParameters Parameters(Scene->SkyLight->OcclusionMaxDistance, Scene->SkyLight->Contrast);
		TRefCountPtr<IPooledRenderTarget> DynamicIrradiance;

		if (Scene->SkyLight->bCastShadows 
			&& !GDistanceFieldAOApplyToStaticIndirect
			&& ShouldRenderDistanceFieldAO() 
			&& ViewFamily.EngineShowFlags.AmbientOcclusion)
		{
			bApplyShadowing = RenderDistanceFieldLighting(RHICmdList, Parameters, VelocityTexture, DynamicBentNormalAO, DynamicIrradiance, false, false, false);
		}

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilRead);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		for( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
		{
			const FViewInfo& View = Views[ViewIndex];

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			if (GAOOverwriteSceneColor)
			{
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			}
			else
			{
				const bool bCheckerboardSubsurfaceRendering = FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering(SceneContext.GetSceneColorFormat());
				if (bCheckerboardSubsurfaceRendering)
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
				}
				else
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
				}
			}

			const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View) && IsValidRef(DynamicIrradiance);
			TShaderMapRef< FPostProcessVS > VertexShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			if (bApplyShadowing)
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<true, true> > PixelShader(View.ShaderMap);

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, View, DynamicBentNormalAO->GetRenderTargetItem().ShaderResourceTexture, DynamicIrradiance, Parameters, Scene->SkyLight);
				}
				else
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<true, false> > PixelShader(View.ShaderMap);

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, View, DynamicBentNormalAO->GetRenderTargetItem().ShaderResourceTexture, DynamicIrradiance, Parameters, Scene->SkyLight);
				}
			}
			else
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<false, true> > PixelShader(View.ShaderMap);

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, View, GWhiteTexture->TextureRHI, NULL, Parameters, Scene->SkyLight);
				}
				else
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<false, false> > PixelShader(View.ShaderMap);

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, View, GWhiteTexture->TextureRHI, NULL, Parameters, Scene->SkyLight);
				}
			}

			DrawRectangle( 
				RHICmdList,
				0, 0, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
				SceneContext.GetBufferSizeXY(),
				*VertexShader);
		}
	}
}
