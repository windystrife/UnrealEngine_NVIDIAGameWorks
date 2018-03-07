// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HeightfieldLighting.cpp
=============================================================================*/

#include "HeightfieldLighting.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "Materials/Material.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "DistanceFieldLightingShared.h"
#include "ScreenRendering.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "LightRendering.h"
#include "PipelineStateCache.h"

// Currently disabled because the bHasHeightfieldRepresentation GBuffer bit has been reallocated, and self-shadowing artifacts are too severe without that bit
int32 GAOHeightfieldOcclusion = 0;
FAutoConsoleVariableRef CVarAOHeightfieldOcclusion(
	TEXT("r.AOHeightfieldOcclusion"),
	GAOHeightfieldOcclusion,
	TEXT("Whether to compute AO from heightfields (landscape)"),
	ECVF_RenderThreadSafe
	);

int32 GHeightfieldGlobalIllumination = 1;
FAutoConsoleVariableRef CVarHeightfieldGlobalIllumination(
	TEXT("r.HeightfieldGlobalIllumination"),
	GHeightfieldGlobalIllumination,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GHeightfieldInnerBounceDistance = 3000;
FAutoConsoleVariableRef CVarHeightfieldInnerBounceDistancer(
	TEXT("r.HeightfieldInnerBounceDistance"),
	GHeightfieldInnerBounceDistance,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GHeightfieldOuterBounceDistanceScale = 3;
FAutoConsoleVariableRef CVarHeightfieldOuterBounceDistanceScale(
	TEXT("r.HeightfieldOuterBounceDistanceScale"),
	GHeightfieldOuterBounceDistanceScale,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GetGHeightfieldBounceDistance()
{
	return GHeightfieldInnerBounceDistance * GHeightfieldOuterBounceDistanceScale;
}

float GHeightfieldTargetUnitsPerTexel = 200;
FAutoConsoleVariableRef CVarHeightfieldTargetUnitsPerTexel(
	TEXT("r.HeightfieldTargetUnitsPerTexel"),
	GHeightfieldTargetUnitsPerTexel,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

void FHeightfieldLightingAtlas::InitDynamicRHI()
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (AtlasSize.GetMin() > 0)
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(
			AtlasSize,
			PF_G16,
			FClearValueBinding::Transparent,
			TexCreate_None,
			TexCreate_RenderTargetable,
			false));

		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Height, TEXT("HeightAtlas"));

		FPooledRenderTargetDesc Desc2(FPooledRenderTargetDesc::Create2DDesc(
			AtlasSize,
			PF_R8G8,
			FClearValueBinding::Transparent,
			TexCreate_None,
			TexCreate_RenderTargetable,
			false));

		GRenderTargetPool.FindFreeElement(RHICmdList, Desc2, Normal, TEXT("NormalAtlas"));

		FPooledRenderTargetDesc Desc3(FPooledRenderTargetDesc::Create2DDesc(
			AtlasSize,
			PF_R8G8B8A8, 
			FClearValueBinding::Transparent,
			TexCreate_None,
			TexCreate_RenderTargetable,
			false));

		GRenderTargetPool.FindFreeElement(RHICmdList, Desc3, DiffuseColor, TEXT("DiffuseColorAtlas"));

		FPooledRenderTargetDesc Desc4(FPooledRenderTargetDesc::Create2DDesc(
			AtlasSize,
			PF_G8,			
			FClearValueBinding::White,
			TexCreate_None,
			TexCreate_RenderTargetable,
			false));

		GRenderTargetPool.FindFreeElement(RHICmdList, Desc4, DirectionalLightShadowing, TEXT("HeightfieldShadowingAtlas"));

		FPooledRenderTargetDesc Desc5(FPooledRenderTargetDesc::Create2DDesc(
			AtlasSize,
			PF_FloatR11G11B10,
			FClearValueBinding::Black,
			TexCreate_None,
			TexCreate_RenderTargetable,
			false));
		Desc5.AutoWritable = false;

		GRenderTargetPool.FindFreeElement(RHICmdList, Desc5, Lighting, TEXT("HeightfieldLightingAtlas"));
	}
}

void FHeightfieldLightingAtlas::ReleaseDynamicRHI()
{
	GRenderTargetPool.FreeUnusedResource(Height);
	GRenderTargetPool.FreeUnusedResource(Normal);
	GRenderTargetPool.FreeUnusedResource(DiffuseColor);
	GRenderTargetPool.FreeUnusedResource(DirectionalLightShadowing);
	GRenderTargetPool.FreeUnusedResource(Lighting);
}

void FHeightfieldLightingAtlas::InitializeForSize(FIntPoint InAtlasSize)
{
	if (InAtlasSize.X > AtlasSize.X || InAtlasSize.Y > AtlasSize.Y)
	{
		AtlasSize.X = FMath::Max(InAtlasSize.X, AtlasSize.X);
		AtlasSize.Y = FMath::Max(InAtlasSize.Y, AtlasSize.Y);

		if (IsInitialized())
		{
			UpdateRHI();
		}
		else
		{
			InitResource();
		}
	}
}

class FSubsectionHeightfieldDescriptionsResource : public FRenderResource
{
public:

	FCPUUpdatedBuffer Data;

	FSubsectionHeightfieldDescriptionsResource()
	{
		Data.Format = PF_A32B32G32R32F;
		// In float4's, must match usf
		Data.Stride = 4;
	}

	virtual void InitDynamicRHI()  override
	{
		Data.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		Data.Release();
	}
};

TGlobalResource<FSubsectionHeightfieldDescriptionsResource> GSubsectionHeightfieldDescriptions;

class FSubsectionHeightfieldDescriptionParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		SubsectionHeightfieldDescriptions.Bind(ParameterMap,TEXT("SubsectionHeightfieldDescriptions"));
	}

	friend FArchive& operator<<(FArchive& Ar,FSubsectionHeightfieldDescriptionParameters& Parameters)
	{
		Ar << Parameters.SubsectionHeightfieldDescriptions;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI)
	{
		SetSRVParameter(RHICmdList, ShaderRHI, SubsectionHeightfieldDescriptions, GSubsectionHeightfieldDescriptions.Data.BufferSRV);
	}

private:
	FShaderResourceParameter SubsectionHeightfieldDescriptions;
};

int32 UploadSubsectionHeightfieldDescriptions(const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions, FVector2D InvLightingAtlasSize, float InvDownsampleFactor)
{
	TArray<FVector4, SceneRenderingAllocator> HeightfieldDescriptionData;
	HeightfieldDescriptionData.Empty(HeightfieldDescriptions.Num() * GSubsectionHeightfieldDescriptions.Data.Stride);

	for (int32 DescriptionIndex = 0; DescriptionIndex < HeightfieldDescriptions.Num(); DescriptionIndex++)
	{
		const FHeightfieldComponentDescription& Description = HeightfieldDescriptions[DescriptionIndex];

		for (int32 SubsectionY = 0; SubsectionY < Description.NumSubsections; SubsectionY++)
		{
			for (int32 SubsectionX = 0; SubsectionX < Description.NumSubsections; SubsectionX++)
			{
				HeightfieldDescriptionData.Add(FVector4(SubsectionX, SubsectionY, 0, 0));
				HeightfieldDescriptionData.Add(Description.SubsectionScaleAndBias);
				HeightfieldDescriptionData.Add(Description.HeightfieldScaleBias);

				// GlobalUVScaleBias = SubsectionSizeQuads / AtlasSize, (SubsectionBase + SubsectionId * SubsectionSizeQuads - AtlasMin) / AtlasSize
				const FVector4 GlobalUVScaleBias(
					Description.SubsectionScaleAndBias.X * InvLightingAtlasSize.X * InvDownsampleFactor, 
					Description.SubsectionScaleAndBias.Y * InvLightingAtlasSize.Y * InvDownsampleFactor, 
					(Description.LightingAtlasLocation.X + SubsectionX * Description.SubsectionScaleAndBias.X * InvDownsampleFactor) * InvLightingAtlasSize.X, 
					(Description.LightingAtlasLocation.Y + SubsectionY * Description.SubsectionScaleAndBias.Y * InvDownsampleFactor) * InvLightingAtlasSize.Y);

				HeightfieldDescriptionData.Add(GlobalUVScaleBias);
			}
		}
	}

	check(HeightfieldDescriptionData.Num() % GSubsectionHeightfieldDescriptions.Data.Stride == 0);

	if (HeightfieldDescriptionData.Num() > GSubsectionHeightfieldDescriptions.Data.MaxElements)
	{
		GSubsectionHeightfieldDescriptions.Data.MaxElements = HeightfieldDescriptionData.Num() * 5 / 4;
		GSubsectionHeightfieldDescriptions.Data.Release();
		GSubsectionHeightfieldDescriptions.Data.Initialize();
	}

	void* LockedBuffer = RHILockVertexBuffer(GSubsectionHeightfieldDescriptions.Data.Buffer, 0, GSubsectionHeightfieldDescriptions.Data.Buffer->GetSize(), RLM_WriteOnly);
	const uint32 MemcpySize = HeightfieldDescriptionData.GetTypeSize() * HeightfieldDescriptionData.Num();
	check(GSubsectionHeightfieldDescriptions.Data.Buffer->GetSize() >= MemcpySize);
	FPlatformMemory::Memcpy(LockedBuffer, HeightfieldDescriptionData.GetData(), MemcpySize);
	RHIUnlockVertexBuffer(GSubsectionHeightfieldDescriptions.Data.Buffer);

	return HeightfieldDescriptionData.Num() / GSubsectionHeightfieldDescriptions.Data.Stride;
}

class FHeightfieldSubsectionQuadVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHeightfieldSubsectionQuadVS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	/** Default constructor. */
	FHeightfieldSubsectionQuadVS() {}

	/** Initialization constructor. */
	FHeightfieldSubsectionQuadVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SubsectionHeightfieldParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SubsectionHeightfieldParameters.Set(RHICmdList, ShaderRHI);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SubsectionHeightfieldParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FSubsectionHeightfieldDescriptionParameters SubsectionHeightfieldParameters;
};

IMPLEMENT_SHADER_TYPE(,FHeightfieldSubsectionQuadVS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("HeightfieldSubsectionQuadVS"),SF_Vertex);

class FInitializeHeightfieldsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FInitializeHeightfieldsPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	/** Default constructor. */
	FInitializeHeightfieldsPS() {}

	/** Initialization constructor. */
	FInitializeHeightfieldsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		HeightfieldTextureParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, UTexture2D* HeightfieldTextureValue, UTexture2D* DiffuseColorTextureValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		HeightfieldTextureParameters.Set(RHICmdList, ShaderRHI, HeightfieldTextureValue, DiffuseColorTextureValue);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HeightfieldTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FHeightfieldTextureParameters HeightfieldTextureParameters;
};

IMPLEMENT_SHADER_TYPE(,FInitializeHeightfieldsPS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("InitializeHeightfieldsPS"),SF_Pixel);

class FQuadVertexBuffer : public FVertexBuffer
{
public:

	FQuadVertexBuffer()
	{
	}

	virtual void InitRHI() override
	{
		const uint32 Size = 6 * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo;

		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, Buffer);		
		FScreenVertex* DestVertex = (FScreenVertex*)Buffer;

		DestVertex[0].Position = FVector2D(1,  1);
		DestVertex[0].UV = FVector2D(1,	1);

		DestVertex[1].Position = FVector2D(0,  1);
		DestVertex[1].UV = FVector2D(0,	1);

		DestVertex[2].Position = FVector2D(1,	0);
		DestVertex[2].UV = FVector2D(1,	0);

		DestVertex[3].Position = FVector2D(1,	0);
		DestVertex[3].UV = FVector2D(1,	0);

		DestVertex[4].Position = FVector2D(0,  1);
		DestVertex[4].UV = FVector2D(0,	1);

		DestVertex[5].Position = FVector2D(0,	0);
		DestVertex[5].UV = FVector2D(0,	0);

		RHIUnlockVertexBuffer(VertexBufferRHI);      
	}
};

TGlobalResource<FQuadVertexBuffer> GQuadVertexBuffer;

bool SupportsHeightfieldLighting(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldGI(ShaderPlatform);
}

bool AllowHeightfieldGI(const FViewInfo& View)
{
	return GHeightfieldGlobalIllumination 
		&& View.State 
		&& GDistanceFieldGI 
		&& View.Family->EngineShowFlags.DistanceFieldGI;
}

void FHeightfieldLightingViewInfo::SetupVisibleHeightfields(const FViewInfo& View, FRHICommandListImmediate& RHICmdList)
{
	const FScene* Scene = (const FScene*)View.Family->Scene;
	const int32 NumPrimitives = Scene->DistanceFieldSceneData.HeightfieldPrimitives.Num();

	if ((AllowHeightfieldGI(View) || GAOHeightfieldOcclusion) 
		&& NumPrimitives > 0
		&& SupportsHeightfieldLighting(View.GetFeatureLevel(), View.GetShaderPlatform()))
	{
		const float MaxDistanceSquared = FMath::Square(GetMaxAOViewDistance() + GetGHeightfieldBounceDistance());
		float LocalToWorldScale = 1;

		for (int32 HeightfieldPrimitiveIndex = 0; HeightfieldPrimitiveIndex < NumPrimitives; HeightfieldPrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* HeightfieldPrimitive = Scene->DistanceFieldSceneData.HeightfieldPrimitives[HeightfieldPrimitiveIndex];
			const FBoxSphereBounds& PrimitiveBounds = HeightfieldPrimitive->Proxy->GetBounds();
			const float DistanceToPrimitiveSq = (PrimitiveBounds.Origin - View.ViewMatrices.GetViewOrigin()).SizeSquared();
						
			if (View.ViewFrustum.IntersectSphere(PrimitiveBounds.Origin, PrimitiveBounds.SphereRadius + GetGHeightfieldBounceDistance())
				&& DistanceToPrimitiveSq < MaxDistanceSquared)
			{
				UTexture2D* HeightfieldTexture = NULL;
				UTexture2D* DiffuseColorTexture = NULL;
				FHeightfieldComponentDescription NewComponentDescription(HeightfieldPrimitive->Proxy->GetLocalToWorld());
				HeightfieldPrimitive->Proxy->GetHeightfieldRepresentation(HeightfieldTexture, DiffuseColorTexture, NewComponentDescription);

				if (HeightfieldTexture && HeightfieldTexture->Resource->TextureRHI)
				{
					const FIntPoint HeightfieldSize = NewComponentDescription.HeightfieldRect.Size();

					if (Heightfield.Rect.Area() == 0)
					{
						Heightfield.Rect = NewComponentDescription.HeightfieldRect;
						LocalToWorldScale = NewComponentDescription.LocalToWorld.GetScaleVector().X;
					}
					else
					{
						Heightfield.Rect.Union(NewComponentDescription.HeightfieldRect);
					}

					TArray<FHeightfieldComponentDescription>& ComponentDescriptions = Heightfield.ComponentDescriptions.FindOrAdd(FHeightfieldComponentTextures(HeightfieldTexture, DiffuseColorTexture));
					ComponentDescriptions.Add(NewComponentDescription);
				}
			}
		}

		if (AllowHeightfieldGI(View) && Heightfield.ComponentDescriptions.Num() > 0)
		{
			FSceneViewState* ViewState = (FSceneViewState*)View.State;

			{
				FHeightfieldLightingAtlas*& ExistingAtlas = ViewState->HeightfieldLightingAtlas;

				if (!ExistingAtlas)
				{
					ExistingAtlas = new FHeightfieldLightingAtlas();
				}

				Heightfield.DownsampleFactor = FMath::Max(FMath::TruncToInt(GHeightfieldTargetUnitsPerTexel / LocalToWorldScale), 1);
				Heightfield.DownsampledRect = Heightfield.Rect / Heightfield.DownsampleFactor;
				Heightfield.Rect.Min = FIntPoint::DivideAndRoundDown(Heightfield.Rect.Min, Heightfield.DownsampleFactor) * Heightfield.DownsampleFactor;
				ExistingAtlas->InitializeForSize(Heightfield.DownsampledRect.Size());

				for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TIterator It(Heightfield.ComponentDescriptions); It; ++It)
				{
					TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();

					for (int32 ComponentIndex = 0; ComponentIndex < HeightfieldDescriptions.Num(); ComponentIndex++)
					{
						FIntPoint RelativeAtlasOffset = HeightfieldDescriptions[ComponentIndex].HeightfieldRect.Min - Heightfield.Rect.Min;
						HeightfieldDescriptions[ComponentIndex].LightingAtlasLocation = FVector2D(RelativeAtlasOffset.X, RelativeAtlasOffset.Y) / Heightfield.DownsampleFactor;
					}
				}

				{
					SCOPED_DRAW_EVENT(RHICmdList, InitializeHeightfield);
					const FIntPoint LightingAtlasSize = ExistingAtlas->GetAtlasSize();
					const FVector2D InvLightingAtlasSize(1.0f / LightingAtlasSize.X, 1.0f / LightingAtlasSize.Y);

					FTextureRHIParamRef RenderTargets[3] =
					{
						ExistingAtlas->Height->GetRenderTargetItem().TargetableTexture,
						ExistingAtlas->Normal->GetRenderTargetItem().TargetableTexture,
						ExistingAtlas->DiffuseColor->GetRenderTargetItem().TargetableTexture
					};

					RHICmdList.SetViewport(0, 0, 0.0f, LightingAtlasSize.X, LightingAtlasSize.Y, 1.0f);
					SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, FTextureRHIParamRef(), ESimpleRenderTargetMode::EClearColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilRead);
					
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					RHICmdList.SetStreamSource(0, GQuadVertexBuffer.VertexBufferRHI, 0);

					TShaderMapRef<FHeightfieldSubsectionQuadVS> VertexShader(View.ShaderMap);
					TShaderMapRef<FInitializeHeightfieldsPS> PixelShader(View.ShaderMap);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TConstIterator It(Heightfield.ComponentDescriptions); It; ++It)
					{
						const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();

						if (HeightfieldDescriptions.Num() > 0)
						{
							const int32 NumQuads = UploadSubsectionHeightfieldDescriptions(HeightfieldDescriptions, InvLightingAtlasSize, 1.0f / Heightfield.DownsampleFactor);

							VertexShader->SetParameters(RHICmdList, View);
							PixelShader->SetParameters(RHICmdList, View, It.Key().HeightAndNormal, It.Key().DiffuseColor);

							RHICmdList.DrawPrimitive(PT_TriangleList, 0, 2, NumQuads);
						}
					}

					RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, RenderTargets, ARRAY_COUNT(RenderTargets));
				}
			}
		}
	}
}

void FHeightfieldLightingViewInfo::SetupHeightfieldsForScene(const FScene& Scene, FRHICommandListImmediate& RHICmdList)
{
	const int32 NumPrimitives = Scene.DistanceFieldSceneData.HeightfieldPrimitives.Num();

	if (NumPrimitives > 0
		&& SupportsHeightfieldLighting(Scene.GetFeatureLevel(), Scene.GetShaderPlatform()))
	{
		const float MaxDistanceSquared = FMath::Square(GetMaxAOViewDistance() + GetGHeightfieldBounceDistance());
		float LocalToWorldScale = 1;

		for (int32 HeightfieldPrimitiveIndex = 0; HeightfieldPrimitiveIndex < NumPrimitives; HeightfieldPrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* HeightfieldPrimitive = Scene.DistanceFieldSceneData.HeightfieldPrimitives[HeightfieldPrimitiveIndex];

			UTexture2D* HeightfieldTexture = NULL;
			UTexture2D* DiffuseColorTexture = NULL;
			FHeightfieldComponentDescription NewComponentDescription(HeightfieldPrimitive->Proxy->GetLocalToWorld());
			HeightfieldPrimitive->Proxy->GetHeightfieldRepresentation(HeightfieldTexture, DiffuseColorTexture, NewComponentDescription);

			if (HeightfieldTexture && HeightfieldTexture->Resource->TextureRHI)
			{
				const FIntPoint HeightfieldSize = NewComponentDescription.HeightfieldRect.Size();

				if (Heightfield.Rect.Area() == 0)
				{
					Heightfield.Rect = NewComponentDescription.HeightfieldRect;
					LocalToWorldScale = NewComponentDescription.LocalToWorld.GetScaleVector().X;
				}
				else
				{
					Heightfield.Rect.Union(NewComponentDescription.HeightfieldRect);
				}

				TArray<FHeightfieldComponentDescription>& ComponentDescriptions = Heightfield.ComponentDescriptions.FindOrAdd(FHeightfieldComponentTextures(HeightfieldTexture, DiffuseColorTexture));
				ComponentDescriptions.Add(NewComponentDescription);
			}
		}
	}
}

class FHeightfieldDescriptionsResource : public FRenderResource
{
public:

	FCPUUpdatedBuffer Data;

	FHeightfieldDescriptionsResource()
	{
		Data.Format = PF_A32B32G32R32F;
		// In float4's, must match usf
		Data.Stride = 12;
	}

	virtual void InitDynamicRHI()  override
	{
		Data.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		Data.Release();
	}
};

TGlobalResource<FHeightfieldDescriptionsResource> GHeightfieldDescriptions;

FShaderResourceViewRHIParamRef GetHeightfieldDescriptionsSRV()
{
	return GHeightfieldDescriptions.Data.BufferSRV;
}

void UploadHeightfieldDescriptions(const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions, FVector2D InvLightingAtlasSize, float InvDownsampleFactor)
{
	TArray<FVector4, SceneRenderingAllocator> HeightfieldDescriptionData;
	HeightfieldDescriptionData.Empty(HeightfieldDescriptions.Num() * GHeightfieldDescriptions.Data.Stride);

	for (int32 DescriptionIndex = 0; DescriptionIndex < HeightfieldDescriptions.Num(); DescriptionIndex++)
	{
		const FHeightfieldComponentDescription& Description = HeightfieldDescriptions[DescriptionIndex];

		FVector4 HeightfieldScaleBias = Description.HeightfieldScaleBias;
		check(HeightfieldScaleBias.X > 0);

		// CalculateHeightfieldOcclusionCS needs to be fixed up if other values are ever supported
		check(Description.NumSubsections == 1 || Description.NumSubsections == 2);

		// Store the presence of subsections in the sign bit
		HeightfieldScaleBias.X *= Description.NumSubsections > 1 ? -1 : 1;

		HeightfieldDescriptionData.Add(HeightfieldScaleBias);
		HeightfieldDescriptionData.Add(Description.MinMaxUV);

		const FVector4 LightingUVScaleBias(
			InvLightingAtlasSize.X * InvDownsampleFactor, 
			InvLightingAtlasSize.Y * InvDownsampleFactor, 
			Description.LightingAtlasLocation.X * InvLightingAtlasSize.X, 
			Description.LightingAtlasLocation.Y * InvLightingAtlasSize.Y);

		HeightfieldDescriptionData.Add(LightingUVScaleBias);
					
		HeightfieldDescriptionData.Add(FVector4(Description.HeightfieldRect.Size().X, Description.HeightfieldRect.Size().Y, InvLightingAtlasSize.X, InvLightingAtlasSize.Y));

		const FMatrix WorldToLocal = Description.LocalToWorld.Inverse();

		HeightfieldDescriptionData.Add(*(FVector4*)&WorldToLocal.M[0]);
		HeightfieldDescriptionData.Add(*(FVector4*)&WorldToLocal.M[1]);
		HeightfieldDescriptionData.Add(*(FVector4*)&WorldToLocal.M[2]);
		HeightfieldDescriptionData.Add(*(FVector4*)&WorldToLocal.M[3]);

		HeightfieldDescriptionData.Add(*(FVector4*)&Description.LocalToWorld.M[0]);
		HeightfieldDescriptionData.Add(*(FVector4*)&Description.LocalToWorld.M[1]);
		HeightfieldDescriptionData.Add(*(FVector4*)&Description.LocalToWorld.M[2]);
		HeightfieldDescriptionData.Add(*(FVector4*)&Description.LocalToWorld.M[3]);
	}

	check(HeightfieldDescriptionData.Num() % GHeightfieldDescriptions.Data.Stride == 0);

	if (HeightfieldDescriptionData.Num() > GHeightfieldDescriptions.Data.MaxElements)
	{
		GHeightfieldDescriptions.Data.MaxElements = HeightfieldDescriptionData.Num() * 5 / 4;
		GHeightfieldDescriptions.Data.Release();
		GHeightfieldDescriptions.Data.Initialize();
	}

	void* LockedBuffer = RHILockVertexBuffer(GHeightfieldDescriptions.Data.Buffer, 0, GHeightfieldDescriptions.Data.Buffer->GetSize(), RLM_WriteOnly);
	const uint32 MemcpySize = HeightfieldDescriptionData.GetTypeSize() * HeightfieldDescriptionData.Num();
	check(GHeightfieldDescriptions.Data.Buffer->GetSize() >= MemcpySize);
	FPlatformMemory::Memcpy(LockedBuffer, HeightfieldDescriptionData.GetData(), MemcpySize);
	RHIUnlockVertexBuffer(GHeightfieldDescriptions.Data.Buffer);
}

class FGlobalHeightfieldParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		GlobalHeightfieldTexture.Bind(ParameterMap,TEXT("GlobalHeightfieldTexture"));
		GlobalNormalTexture.Bind(ParameterMap,TEXT("GlobalNormalTexture"));
		GlobalDiffuseColorTexture.Bind(ParameterMap,TEXT("GlobalDiffuseColorTexture"));
		GlobalHeightfieldSampler.Bind(ParameterMap,TEXT("GlobalHeightfieldSampler"));
	}

	friend FArchive& operator<<(FArchive& Ar,FGlobalHeightfieldParameters& Parameters)
	{
		Ar << Parameters.GlobalHeightfieldTexture;
		Ar << Parameters.GlobalNormalTexture;
		Ar << Parameters.GlobalDiffuseColorTexture;
		Ar << Parameters.GlobalHeightfieldSampler;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FHeightfieldLightingAtlas& Atlas)
	{
		SetTextureParameter(RHICmdList, ShaderRHI, GlobalHeightfieldTexture, GlobalHeightfieldSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), Atlas.Height->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, GlobalNormalTexture, GlobalHeightfieldSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), Atlas.Normal->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, GlobalDiffuseColorTexture, GlobalHeightfieldSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), Atlas.DiffuseColor->GetRenderTargetItem().ShaderResourceTexture);
	}

private:
	FShaderResourceParameter GlobalHeightfieldTexture;
	FShaderResourceParameter GlobalNormalTexture;
	FShaderResourceParameter GlobalDiffuseColorTexture;
	FShaderResourceParameter GlobalHeightfieldSampler;
};

class FHeightfieldComponentQuadVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHeightfieldComponentQuadVS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	/** Default constructor. */
	FHeightfieldComponentQuadVS() {}

	/** Initialization constructor. */
	FHeightfieldComponentQuadVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		HeightfieldDescriptionParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 NumHeightfieldsValue)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		HeightfieldDescriptionParameters.Set(RHICmdList, ShaderRHI, GetHeightfieldDescriptionsSRV(), NumHeightfieldsValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HeightfieldDescriptionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FHeightfieldDescriptionParameters HeightfieldDescriptionParameters;
};

IMPLEMENT_SHADER_TYPE(,FHeightfieldComponentQuadVS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("HeightfieldComponentQuadVS"),SF_Vertex);

class FShadowHeightfieldsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowHeightfieldsPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	/** Default constructor. */
	FShadowHeightfieldsPS() {}

	/** Initialization constructor. */
	FShadowHeightfieldsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		HeightfieldDescriptionParameters.Bind(Initializer.ParameterMap);
		GlobalHeightfieldParameters.Bind(Initializer.ParameterMap);
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		ShadowmapMinMax.Bind(Initializer.ParameterMap, TEXT("ShadowmapMinMax"));
		ShadowDepthBias.Bind(Initializer.ParameterMap, TEXT("ShadowDepthBias"));
		CascadeDepthMinMax.Bind(Initializer.ParameterMap, TEXT("CascadeDepthMinMax"));
		ShadowDepthTexture.Bind(Initializer.ParameterMap, TEXT("ShadowDepthTexture"));
		ShadowDepthTextureSampler.Bind(Initializer.ParameterMap, TEXT("ShadowDepthTextureSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ProjectedShadowInfo, int32 NumHeightfieldsValue, const FHeightfieldLightingAtlas& Atlas)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		HeightfieldDescriptionParameters.Set(RHICmdList, ShaderRHI, GetHeightfieldDescriptionsSRV(), NumHeightfieldsValue);
		GlobalHeightfieldParameters.Set(RHICmdList, ShaderRHI, Atlas);

		FVector4 ShadowmapMinMaxValue;
		FMatrix WorldToShadowMatrixValue = ProjectedShadowInfo->GetWorldToShadowMatrix(ShadowmapMinMaxValue);

		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);
		SetShaderValue(RHICmdList, ShaderRHI, ShadowmapMinMax, ShadowmapMinMaxValue);
		SetShaderValue(RHICmdList, ShaderRHI, ShadowDepthBias, ProjectedShadowInfo->GetShaderDepthBias());
		SetShaderValue(RHICmdList, ShaderRHI, CascadeDepthMinMax, FVector2D(ProjectedShadowInfo->CascadeSettings.SplitNear, ProjectedShadowInfo->CascadeSettings.SplitFar));

		FTextureRHIParamRef ShadowDepthTextureValue = ProjectedShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture.GetReference();
		FSamplerStateRHIParamRef DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

		SetTextureParameter(RHICmdList, ShaderRHI, ShadowDepthTexture, ShadowDepthTextureSampler, DepthSamplerState, ShadowDepthTextureValue);	
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HeightfieldDescriptionParameters;
		Ar << GlobalHeightfieldParameters;
		Ar << WorldToShadow;
		Ar << ShadowmapMinMax;
		Ar << ShadowDepthBias;
		Ar << CascadeDepthMinMax;
		Ar << ShadowDepthTexture;
		Ar << ShadowDepthTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FHeightfieldDescriptionParameters HeightfieldDescriptionParameters;
	FGlobalHeightfieldParameters GlobalHeightfieldParameters;
	FShaderParameter WorldToShadow;
	FShaderParameter ShadowmapMinMax;
	FShaderParameter ShadowDepthBias;
	FShaderParameter CascadeDepthMinMax;
	FShaderResourceParameter ShadowDepthTexture;
	FShaderResourceParameter ShadowDepthTextureSampler;
};

IMPLEMENT_SHADER_TYPE(,FShadowHeightfieldsPS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("ShadowHeightfieldsPS"),SF_Pixel);

void FHeightfieldLightingViewInfo::ClearShadowing(const FViewInfo& View, FRHICommandListImmediate& RHICmdList, const FLightSceneInfo& LightSceneInfo) const
{
	if (AllowHeightfieldGI(View)
		&& SupportsHeightfieldLighting(View.GetFeatureLevel(), View.GetShaderPlatform())
		&& Heightfield.ComponentDescriptions.Num() > 0
		&& LightSceneInfo.Proxy->GetLightType() == LightType_Directional
		&& LightSceneInfo.Proxy->CastsDynamicShadow())
	{
		FSceneViewState* ViewState = (FSceneViewState*)View.State;
		const FHeightfieldLightingAtlas& Atlas = *ViewState->HeightfieldLightingAtlas;
		SetRenderTarget(RHICmdList, Atlas.DirectionalLightShadowing->GetRenderTargetItem().TargetableTexture, NULL, ESimpleRenderTargetMode::EClearColorExistingDepth);		
	}
}

void FHeightfieldLightingViewInfo::ComputeShadowMapShadowing(const FViewInfo& View, FRHICommandListImmediate& RHICmdList, const FProjectedShadowInfo* ProjectedShadowInfo) const
{
	if (AllowHeightfieldGI(View)
		&& SupportsHeightfieldLighting(View.GetFeatureLevel(), View.GetShaderPlatform())
		&& Heightfield.ComponentDescriptions.Num() > 0
		&& ProjectedShadowInfo->IsWholeSceneDirectionalShadow()
		&& ProjectedShadowInfo->DependentView == &View
		&& !ProjectedShadowInfo->bRayTracedDistanceField)
	{
		SCOPED_DRAW_EVENT(RHICmdList, HeightfieldShadowMapShadowingForGI);

		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		{
			const FHeightfieldLightingAtlas& Atlas = *ViewState->HeightfieldLightingAtlas;

			const FIntPoint LightingAtlasSize = Atlas.GetAtlasSize();
			const FVector2D InvLightingAtlasSize(1.0f / LightingAtlasSize.X, 1.0f / LightingAtlasSize.Y);
			SetRenderTarget(RHICmdList, Atlas.DirectionalLightShadowing->GetRenderTargetItem().TargetableTexture, NULL);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(0, 0, 0.0f, LightingAtlasSize.X, LightingAtlasSize.Y, 1.0f);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			// Combine with other shadow types with min (ray traced)
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RED, BO_Min, BF_One, BF_One>::GetRHI();
			RHICmdList.SetStreamSource(0, GQuadVertexBuffer.VertexBufferRHI, 0);

			TShaderMapRef<FHeightfieldComponentQuadVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FShadowHeightfieldsPS> PixelShader(View.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TConstIterator It(Heightfield.ComponentDescriptions); It; ++It)
			{
				const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();

				if (HeightfieldDescriptions.Num() > 0)
				{
					//@todo - cull heightfield tiles with shadow bounds
					UploadHeightfieldDescriptions(HeightfieldDescriptions, InvLightingAtlasSize, 1.0f / Heightfield.DownsampleFactor);

					VertexShader->SetParameters(RHICmdList, View, HeightfieldDescriptions.Num());
					PixelShader->SetParameters(RHICmdList, View, ProjectedShadowInfo, HeightfieldDescriptions.Num(), Atlas);

					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 2, HeightfieldDescriptions.Num());
				}
			}
		}
	}
}


class FRayTracedShadowHeightfieldsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRayTracedShadowHeightfieldsPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLightTileIntersectionParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FRayTracedShadowHeightfieldsPS() {}

	/** Initialization constructor. */
	FRayTracedShadowHeightfieldsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		HeightfieldDescriptionParameters.Bind(Initializer.ParameterMap);
		GlobalHeightfieldParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		TanLightAngle.Bind(Initializer.ParameterMap, TEXT("TanLightAngle"));
		CascadeDepthMinMax.Bind(Initializer.ParameterMap, TEXT("CascadeDepthMinMax"));
		LightTileIntersectionParameters.Bind(Initializer.ParameterMap);
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		TwoSidedMeshDistanceBias.Bind(Initializer.ParameterMap, TEXT("TwoSidedMeshDistanceBias"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FProjectedShadowInfo* ProjectedShadowInfo, 
		int32 NumHeightfieldsValue, 
		const FHeightfieldLightingAtlas& Atlas, 
		FLightTileIntersectionResources* TileIntersectionResources, 
		FDistanceFieldObjectBufferResource& CulledObjectBuffers)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		HeightfieldDescriptionParameters.Set(RHICmdList, ShaderRHI, GetHeightfieldDescriptionsSRV(), NumHeightfieldsValue);
		GlobalHeightfieldParameters.Set(RHICmdList, ShaderRHI, Atlas);
		ObjectParameters.Set(RHICmdList, ShaderRHI, CulledObjectBuffers.Buffers);

		SetShaderValue(RHICmdList, ShaderRHI, LightDirection, ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetDirection());

		const float LightSourceAngle = FMath::Clamp(ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetLightSourceAngle(), 0.001f, 5.0f) * PI / 180.0f;
		SetShaderValue(RHICmdList, ShaderRHI, TanLightAngle, FMath::Tan(LightSourceAngle));

		SetShaderValue(RHICmdList, ShaderRHI, CascadeDepthMinMax, FVector2D(ProjectedShadowInfo->CascadeSettings.SplitNear, ProjectedShadowInfo->CascadeSettings.SplitFar));

		check(TileIntersectionResources || !LightTileIntersectionParameters.IsBound());

		if (TileIntersectionResources)
		{
			LightTileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);
		}

		FMatrix WorldToShadowMatrixValue = FTranslationMatrix(ProjectedShadowInfo->PreShadowTranslation) * ProjectedShadowInfo->SubjectAndReceiverMatrix;
		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);

		extern float GTwoSidedMeshDistanceBias;
		SetShaderValue(RHICmdList, ShaderRHI, TwoSidedMeshDistanceBias, GTwoSidedMeshDistanceBias);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HeightfieldDescriptionParameters;
		Ar << GlobalHeightfieldParameters;
		Ar << ObjectParameters;
		Ar << LightDirection;
		Ar << TanLightAngle;
		Ar << CascadeDepthMinMax;
		Ar << LightTileIntersectionParameters;
		Ar << WorldToShadow;
		Ar << TwoSidedMeshDistanceBias;
		return bShaderHasOutdatedParameters;
	}

private:

	FHeightfieldDescriptionParameters HeightfieldDescriptionParameters;
	FGlobalHeightfieldParameters GlobalHeightfieldParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FShaderParameter LightDirection;
	FShaderParameter TanLightAngle;
	FShaderParameter CascadeDepthMinMax;
	FLightTileIntersectionParameters LightTileIntersectionParameters;
	FShaderParameter WorldToShadow;
	FShaderParameter TwoSidedMeshDistanceBias;
};

IMPLEMENT_SHADER_TYPE(,FRayTracedShadowHeightfieldsPS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("RayTracedShadowHeightfieldsPS"),SF_Pixel);


void FHeightfieldLightingViewInfo::ComputeRayTracedShadowing(
	const FViewInfo& View, 
	FRHICommandListImmediate& RHICmdList, 
	const FProjectedShadowInfo* ProjectedShadowInfo, 
	FLightTileIntersectionResources* TileIntersectionResources,
	FDistanceFieldObjectBufferResource& CulledObjectBuffers) const
{
	if (AllowHeightfieldGI(View)
		&& SupportsHeightfieldLighting(View.GetFeatureLevel(), View.GetShaderPlatform())
		&& Heightfield.ComponentDescriptions.Num() > 0
		&& ProjectedShadowInfo->IsWholeSceneDirectionalShadow()
		&& ProjectedShadowInfo->DependentView == &View
		&& ProjectedShadowInfo->bRayTracedDistanceField)
	{
		SCOPED_DRAW_EVENT(RHICmdList, HeightfieldRayTracedShadowingForGI);

		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		{
			const FHeightfieldLightingAtlas& Atlas = *ViewState->HeightfieldLightingAtlas;

			const FIntPoint LightingAtlasSize = Atlas.GetAtlasSize();
			const FVector2D InvLightingAtlasSize(1.0f / LightingAtlasSize.X, 1.0f / LightingAtlasSize.Y);
			SetRenderTarget(RHICmdList, Atlas.DirectionalLightShadowing->GetRenderTargetItem().TargetableTexture, NULL);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(0, 0, 0.0f, LightingAtlasSize.X, LightingAtlasSize.Y, 1.0f);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			// Combine with other shadow types with min (CSM)
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RED, BO_Min, BF_One, BF_One>::GetRHI();
			RHICmdList.SetStreamSource(0, GQuadVertexBuffer.VertexBufferRHI, 0);

			TShaderMapRef<FHeightfieldComponentQuadVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FRayTracedShadowHeightfieldsPS> PixelShader(View.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TConstIterator It(Heightfield.ComponentDescriptions); It; ++It)
			{
				const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();

				if (HeightfieldDescriptions.Num() > 0)
				{
					UploadHeightfieldDescriptions(HeightfieldDescriptions, InvLightingAtlasSize, 1.0f / Heightfield.DownsampleFactor);

					VertexShader->SetParameters(RHICmdList, View, HeightfieldDescriptions.Num());
					PixelShader->SetParameters(RHICmdList, View, ProjectedShadowInfo, HeightfieldDescriptions.Num(), Atlas, TileIntersectionResources, CulledObjectBuffers);

					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 2, HeightfieldDescriptions.Num());
				}
			}
		}
	}
}

class FLightHeightfieldsPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLightHeightfieldsPS, Material);
public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("APPLY_LIGHT_FUNCTION"), 1);
	}

	/** Default constructor. */
	FLightHeightfieldsPS() {}

	/** Initialization constructor. */
	FLightHeightfieldsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		HeightfieldDescriptionParameters.Bind(Initializer.ParameterMap);
		GlobalHeightfieldParameters.Bind(Initializer.ParameterMap);
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		LightColor.Bind(Initializer.ParameterMap, TEXT("LightColor"));
		SkyLightIndirectScale.Bind(Initializer.ParameterMap, TEXT("SkyLightIndirectScale"));
		HeightfieldShadowing.Bind(Initializer.ParameterMap, TEXT("HeightfieldShadowing"));
		HeightfieldShadowingSampler.Bind(Initializer.ParameterMap, TEXT("HeightfieldShadowingSampler"));
		WorldToLight.Bind(Initializer.ParameterMap, TEXT("WorldToLight"));
		LightFunctionParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FLightSceneInfo& LightSceneInfo, 
		const FMaterialRenderProxy* MaterialProxy,
		int32 NumHeightfieldsValue, 
		const FHeightfieldLightingAtlas& Atlas,
		float SkyLightIndirectScaleValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, true, ESceneRenderTargetsMode::SetTextures);

		HeightfieldDescriptionParameters.Set(RHICmdList, ShaderRHI, GetHeightfieldDescriptionsSRV(), NumHeightfieldsValue);
		GlobalHeightfieldParameters.Set(RHICmdList, ShaderRHI, Atlas);

		SetShaderValue(RHICmdList, ShaderRHI, LightDirection, LightSceneInfo.Proxy->GetDirection());
		SetShaderValue(RHICmdList, ShaderRHI, LightColor, LightSceneInfo.Proxy->GetColor() * LightSceneInfo.Proxy->GetIndirectLightingScale());

		SetShaderValue(RHICmdList, ShaderRHI, SkyLightIndirectScale, SkyLightIndirectScaleValue);

		const FSceneViewState* ViewState = (const FSceneViewState*)View.State;
		SetTextureParameter(RHICmdList, ShaderRHI, HeightfieldShadowing, HeightfieldShadowingSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), Atlas.DirectionalLightShadowing->GetRenderTargetItem().ShaderResourceTexture);
	
		const FVector Scale = LightSceneInfo.Proxy->GetLightFunctionScale();
		// Switch x and z so that z of the user specified scale affects the distance along the light direction
		const FVector InverseScale = FVector( 1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X );
		const FMatrix WorldToLightValue = LightSceneInfo.Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));	

		SetShaderValue(RHICmdList, ShaderRHI, WorldToLight, WorldToLightValue);

		LightFunctionParameters.Set(RHICmdList, ShaderRHI, &LightSceneInfo, 1);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << HeightfieldDescriptionParameters;
		Ar << GlobalHeightfieldParameters;
		Ar << LightDirection;
		Ar << LightColor;
		Ar << SkyLightIndirectScale;
		Ar << HeightfieldShadowing;
		Ar << HeightfieldShadowingSampler;
		Ar << WorldToLight;
		Ar << LightFunctionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FHeightfieldDescriptionParameters HeightfieldDescriptionParameters;
	FGlobalHeightfieldParameters GlobalHeightfieldParameters;
	FShaderParameter LightDirection;
	FShaderParameter LightColor;
	FShaderParameter SkyLightIndirectScale;
	FShaderResourceParameter HeightfieldShadowing;
	FShaderResourceParameter HeightfieldShadowingSampler;
	FShaderParameter WorldToLight;
	FLightFunctionSharedParameters LightFunctionParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FLightHeightfieldsPS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("LightHeightfieldsPS"),SF_Pixel);

void FHeightfieldLightingViewInfo::ComputeLighting(const FViewInfo& View, FRHICommandListImmediate& RHICmdList, const FLightSceneInfo& LightSceneInfo) const
{
	const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();

	if (AllowHeightfieldGI(View) 
		&& SupportsHeightfieldLighting(FeatureLevel, View.GetShaderPlatform())
		//@todo - handle local lights
		&& LightSceneInfo.Proxy->GetLightType() == LightType_Directional
		&& LightSceneInfo.Proxy->CastsDynamicShadow()
		&& Heightfield.ComponentDescriptions.Num() > 0)
	{
		SCOPED_DRAW_EVENT(RHICmdList, HeightfieldLightingForGI);

		FSceneViewState* ViewState = (FSceneViewState*)View.State;
		const FHeightfieldLightingAtlas& Atlas = *ViewState->HeightfieldLightingAtlas;

		const FIntPoint LightingAtlasSize = Atlas.GetAtlasSize();
		const FVector2D InvLightingAtlasSize(1.0f / LightingAtlasSize.X, 1.0f / LightingAtlasSize.Y);

		SetRenderTarget(RHICmdList, Atlas.Lighting->GetRenderTargetItem().TargetableTexture, NULL, ESimpleRenderTargetMode::EClearColorExistingDepth);		

		const bool bApplyLightFunction = (View.Family->EngineShowFlags.LightFunctions &&
			LightSceneInfo.Proxy->GetLightFunctionMaterial() && 
			LightSceneInfo.Proxy->GetLightFunctionMaterial()->GetMaterial(FeatureLevel)->IsLightFunction());

		const FMaterialRenderProxy* MaterialProxy = bApplyLightFunction ? 
			LightSceneInfo.Proxy->GetLightFunctionMaterial() : 
			UMaterial::GetDefaultMaterial(MD_LightFunction)->GetRenderProxy(false);

		const FScene* Scene = (const FScene*)View.Family->Scene;

		const float SkyLightIndirectScale = ShouldRenderDeferredDynamicSkyLight(Scene, *View.Family) ? Scene->SkyLight->IndirectLightingIntensity : 0;

		// Skip rendering if the DefaultLightFunctionMaterial isn't compiled yet
		if (MaterialProxy->GetMaterial(FeatureLevel)->IsLightFunction())
		{
			RHICmdList.SetViewport(0, 0, 0.0f, LightingAtlasSize.X, LightingAtlasSize.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			RHICmdList.SetStreamSource(0, GQuadVertexBuffer.VertexBufferRHI, 0);

			TShaderMapRef<FHeightfieldComponentQuadVS> VertexShader(View.ShaderMap);

			const FMaterial* Material = MaterialProxy->GetMaterial(FeatureLevel);
			const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
			FLightHeightfieldsPS* PixelShader = MaterialShaderMap->GetShader<FLightHeightfieldsPS>();

			for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TConstIterator It(Heightfield.ComponentDescriptions); It; ++It)
			{
				const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();

				if (HeightfieldDescriptions.Num() > 0)
				{
					UploadHeightfieldDescriptions(HeightfieldDescriptions, InvLightingAtlasSize, 1.0f / Heightfield.DownsampleFactor);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					VertexShader->SetParameters(RHICmdList, View, HeightfieldDescriptions.Num());
					PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, MaterialProxy, HeightfieldDescriptions.Num(), Atlas, SkyLightIndirectScale);

					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 2, HeightfieldDescriptions.Num());
				}
			}
		}
	}
}

const int32 GHeightfieldOcclusionDispatchSize = 8;

class FCalculateHeightfieldOcclusionScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCalculateHeightfieldOcclusionScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);

		OutEnvironment.SetDefine(TEXT("HEIGHTFIELD_OCCLUSION_DISPATCH_SIZEX"), GHeightfieldOcclusionDispatchSize);
		extern int32 GConeTraceDownsampleFactor;
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FCalculateHeightfieldOcclusionScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		HeightfieldDescriptionParameters.Bind(Initializer.ParameterMap);
		HeightfieldTextureParameters.Bind(Initializer.ParameterMap);
		ScreenGridConeVisibility.Bind(Initializer.ParameterMap, TEXT("ScreenGridConeVisibility"));
		TanConeHalfAngle.Bind(Initializer.ParameterMap, TEXT("TanConeHalfAngle"));
	}

	FCalculateHeightfieldOcclusionScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		UTexture2D* HeightfieldTextureValue, 
		int32 NumHeightfieldsValue,
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FAOScreenGridResources& ScreenGridResources,
		const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);
		HeightfieldDescriptionParameters.Set(RHICmdList, ShaderRHI, GetHeightfieldDescriptionsSRV(), NumHeightfieldsValue);
		HeightfieldTextureParameters.Set(RHICmdList, ShaderRHI, HeightfieldTextureValue, NULL);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.ScreenGridConeVisibility.UAV);
		ScreenGridConeVisibility.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources.ScreenGridConeVisibility);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(View.Family->FrameNumber, SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		extern float GAOConeHalfAngle;
		SetShaderValue(RHICmdList, ShaderRHI, TanConeHalfAngle, FMath::Tan(GAOConeHalfAngle));
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FAOScreenGridResources& ScreenGridResources)
	{
		ScreenGridConeVisibility.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.ScreenGridConeVisibility.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << ScreenGridParameters;
		Ar << HeightfieldDescriptionParameters;
		Ar << HeightfieldTextureParameters;
		Ar << ScreenGridConeVisibility;
		Ar << TanConeHalfAngle;
		return bShaderHasOutdatedParameters;
	}

private:

	FAOParameters AOParameters;
	FScreenGridParameters ScreenGridParameters;
	FHeightfieldDescriptionParameters HeightfieldDescriptionParameters;
	FHeightfieldTextureParameters HeightfieldTextureParameters;
	FRWShaderParameter ScreenGridConeVisibility;
	FShaderParameter TanConeHalfAngle;
};

IMPLEMENT_SHADER_TYPE(,FCalculateHeightfieldOcclusionScreenGridCS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("CalculateHeightfieldOcclusionScreenGridCS"),SF_Compute);


void FHeightfieldLightingViewInfo::ComputeOcclusionForScreenGrid(
	const FViewInfo& View, 
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderTargetItem& DistanceFieldNormal, 
	const FAOScreenGridResources& ScreenGridResources,
	const FDistanceFieldAOParameters& Parameters) const
{
	const FScene* Scene = (const FScene*)View.Family->Scene;

	if (Heightfield.ComponentDescriptions.Num() > 0 && GAOHeightfieldOcclusion)
	{
		SCOPED_DRAW_EVENT(RHICmdList, HeightfieldOcclusion);

		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		{
			for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TConstIterator It(Heightfield.ComponentDescriptions); It; ++It)
			{
				const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();

				if (HeightfieldDescriptions.Num() > 0)
				{
					UploadHeightfieldDescriptions(HeightfieldDescriptions, FVector2D(1, 1), 1.0f / Heightfield.DownsampleFactor);

					UTexture2D* HeightfieldTexture = It.Key().HeightAndNormal;

					const uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GHeightfieldOcclusionDispatchSize);
					const uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GHeightfieldOcclusionDispatchSize);

					TShaderMapRef<FCalculateHeightfieldOcclusionScreenGridCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, HeightfieldTexture, HeightfieldDescriptions.Num(), DistanceFieldNormal, ScreenGridResources, Parameters);
					DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
					ComputeShader->UnsetParameters(RHICmdList, ScreenGridResources);
				}
			}
		}
	}
}

class FCalculateHeightfieldIrradianceScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCalculateHeightfieldIrradianceScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);

		OutEnvironment.SetDefine(TEXT("HEIGHTFIELD_OCCLUSION_DISPATCH_SIZEX"), GHeightfieldOcclusionDispatchSize);
		extern int32 GConeTraceDownsampleFactor;
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FCalculateHeightfieldIrradianceScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		HeightfieldDescriptionParameters.Bind(Initializer.ParameterMap);
		HeightfieldIrradiance.Bind(Initializer.ParameterMap, TEXT("HeightfieldIrradiance"));
		TanConeHalfAngle.Bind(Initializer.ParameterMap, TEXT("TanConeHalfAngle"));

		GlobalHeightfieldParameters.Bind(Initializer.ParameterMap);
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		HeightfieldLighting.Bind(Initializer.ParameterMap, TEXT("HeightfieldLighting"));
		HeightfieldLightingSampler.Bind(Initializer.ParameterMap, TEXT("HeightfieldLightingSampler"));
		InnerLightTransferDistance.Bind(Initializer.ParameterMap, TEXT("InnerLightTransferDistance"));
		OuterLightTransferDistanceScale.Bind(Initializer.ParameterMap, TEXT("OuterLightTransferDistanceScale"));
		RecordConeVisibility.Bind(Initializer.ParameterMap, TEXT("RecordConeVisibility"));
	}

	FCalculateHeightfieldIrradianceScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		int32 NumHeightfieldsValue,
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FAOScreenGridResources& ScreenGridResources,
		const FDistanceFieldAOParameters& Parameters,
		const FHeightfieldLightingAtlas& Atlas)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);
		HeightfieldDescriptionParameters.Set(RHICmdList, ShaderRHI, GetHeightfieldDescriptionsSRV(), NumHeightfieldsValue);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.HeightfieldIrradiance.UAV);
		HeightfieldIrradiance.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources.HeightfieldIrradiance);

		extern float GAOConeHalfAngle;
		SetShaderValue(RHICmdList, ShaderRHI, TanConeHalfAngle, FMath::Tan(GAOConeHalfAngle));

		GlobalHeightfieldParameters.Set(RHICmdList, ShaderRHI, Atlas);

		{
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
		}

		const FSceneViewState* ViewState = (const FSceneViewState*)View.State;
		SetTextureParameter(RHICmdList, ShaderRHI, HeightfieldLighting, HeightfieldLightingSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), Atlas.Lighting->GetRenderTargetItem().ShaderResourceTexture);
	
		SetShaderValue(RHICmdList, ShaderRHI, InnerLightTransferDistance, GHeightfieldInnerBounceDistance);
		SetShaderValue(RHICmdList, ShaderRHI, OuterLightTransferDistanceScale, GHeightfieldOuterBounceDistanceScale);

		SetSRVParameter(RHICmdList, ShaderRHI, RecordConeVisibility, ScreenGridResources.ConeDepthVisibilityFunction.SRV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FAOScreenGridResources& ScreenGridResources)
	{
		HeightfieldIrradiance.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.HeightfieldIrradiance.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << ScreenGridParameters;
		Ar << HeightfieldDescriptionParameters;
		Ar << HeightfieldIrradiance;
		Ar << TanConeHalfAngle;
		Ar << GlobalHeightfieldParameters;
		Ar << BentNormalNormalizeFactor;
		Ar << HeightfieldLighting;
		Ar << HeightfieldLightingSampler;
		Ar << InnerLightTransferDistance;
		Ar << OuterLightTransferDistanceScale;
		Ar << RecordConeVisibility;
		return bShaderHasOutdatedParameters;
	}

private:

	FAOParameters AOParameters;
	FScreenGridParameters ScreenGridParameters;
	FHeightfieldDescriptionParameters HeightfieldDescriptionParameters;
	FRWShaderParameter HeightfieldIrradiance;
	FShaderParameter TanConeHalfAngle;

	FGlobalHeightfieldParameters GlobalHeightfieldParameters;
	FShaderParameter BentNormalNormalizeFactor;
	FShaderResourceParameter HeightfieldLighting;
	FShaderResourceParameter HeightfieldLightingSampler;
	FShaderParameter InnerLightTransferDistance;
	FShaderParameter OuterLightTransferDistanceScale;
	FShaderResourceParameter RecordConeVisibility;
};

IMPLEMENT_SHADER_TYPE(,FCalculateHeightfieldIrradianceScreenGridCS,TEXT("/Engine/Private/HeightfieldLighting.usf"),TEXT("CalculateHeightfieldIrradianceScreenGridCS"),SF_Compute);

void FHeightfieldLightingViewInfo::ComputeIrradianceForScreenGrid(
	const FViewInfo& View, 
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderTargetItem& DistanceFieldNormal, 
	const FAOScreenGridResources& ScreenGridResources,
	const FDistanceFieldAOParameters& Parameters) const
{
	const FScene* Scene = (const FScene*)View.Family->Scene;

	if (Heightfield.ComponentDescriptions.Num() > 0 
		&& AllowHeightfieldGI(View)
		&& SupportsHeightfieldLighting(View.GetFeatureLevel(), View.GetShaderPlatform()))
	{
		SCOPED_DRAW_EVENT(RHICmdList, HeightfieldIrradiance);

		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		{
			const FHeightfieldLightingAtlas& Atlas = *ViewState->HeightfieldLightingAtlas;

			const FIntPoint LightingAtlasSize = Atlas.GetAtlasSize();
			const FVector2D InvLightingAtlasSize(1.0f / LightingAtlasSize.X, 1.0f / LightingAtlasSize.Y);

			TArray<FHeightfieldComponentDescription> CombinedHeightfieldDescriptions;

			for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TConstIterator It(Heightfield.ComponentDescriptions); It; ++It)
			{
				const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();
				CombinedHeightfieldDescriptions.Append(HeightfieldDescriptions);
			}

			if (CombinedHeightfieldDescriptions.Num() > 0)
			{
				UploadHeightfieldDescriptions(CombinedHeightfieldDescriptions, InvLightingAtlasSize, 1.0f / Heightfield.DownsampleFactor);

				{
					TShaderMapRef<FCalculateHeightfieldIrradianceScreenGridCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, CombinedHeightfieldDescriptions.Num(), DistanceFieldNormal, ScreenGridResources, Parameters, Atlas);

					const uint32 GroupSizeX = View.ViewRect.Size().X / GAODownsampleFactor;
					const uint32 GroupSizeY = View.ViewRect.Size().Y / GAODownsampleFactor;

					DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

					ComputeShader->UnsetParameters(RHICmdList, ScreenGridResources);
				}
			}
		}
	}
}
