// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
FlexFluidSurfaceRendering.cpp: Flex fluid surface rendering implementation.
=============================================================================*/

#include "FlexFluidSurfaceRendering.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"
#include "RenderingCompositionGraph.h"
#include "ParticleHelper.h"
#include "FlexFluidSurfaceSceneProxy.h"
#include "PhysicsEngine/FlexFluidSurfaceComponent.h"
#include "PipelineStateCache.h"

FFlexFluidSurfaceRenderer GFlexFluidSurfaceRenderer;

/*=============================================================================
Helper
=============================================================================*/

static inline const FTexture2DRHIRef& GetSurface(TRefCountPtr<IPooledRenderTarget>& RenderTarget)
{
	return (const FTexture2DRHIRef&)RenderTarget->GetRenderTargetItem().TargetableTexture;
}

static inline const FTexture2DRHIRef& GetTexture(TRefCountPtr<IPooledRenderTarget>& RenderTarget)
{
	return (const FTexture2DRHIRef&)RenderTarget->GetRenderTargetItem().ShaderResourceTexture;
}

/*=============================================================================
FAnisotropyResources
=============================================================================*/

class FAnisotropyResources : public FRenderResource
{
public:

	FAnisotropyResources() :
		MaxParticles(0)
	{
	}

	virtual void InitDynamicRHI()
	{
		if (MaxParticles > 0)
		{
			AnisoBuffer1.Initialize(sizeof(FVector4), MaxParticles, PF_A32B32G32R32F, BUF_Volatile);
			AnisoBuffer2.Initialize(sizeof(FVector4), MaxParticles, PF_A32B32G32R32F, BUF_Volatile);
			AnisoBuffer3.Initialize(sizeof(FVector4), MaxParticles, PF_A32B32G32R32F, BUF_Volatile);
		}
	}

	virtual void ReleaseDynamicRHI()
	{
		if (AnisoBuffer1.NumBytes > 0)
		{
			AnisoBuffer1.Release();
			AnisoBuffer2.Release();
			AnisoBuffer3.Release();
		}
	}

	void AllocateFor(int32 InMaxParticles)
	{
		if (InMaxParticles > MaxParticles)
		{
			if (!IsInitialized())
			{
				InitResource();
			}
			MaxParticles = InMaxParticles;
			UpdateRHI();
		}
	}

	int32 MaxParticles;

	FReadBuffer AnisoBuffer1;
	FReadBuffer AnisoBuffer2;
	FReadBuffer AnisoBuffer3;
};

TGlobalResource<FAnisotropyResources> GAnisotropyResources;

/*=============================================================================
FFlexFluidSurfaceSpriteBaseVS
=============================================================================*/

class FFlexFluidSurfaceSpriteBaseVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceSpriteBaseVS, MeshMaterial);
	FFlexFluidSurfaceSpriteBaseVS() {}

public:
	FFlexFluidSurfaceSpriteBaseVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		ParticleSizeScale.Bind(Initializer.ParameterMap, TEXT("ParticleSizeScale"));
		AnisotropyBuffer1.Bind(Initializer.ParameterMap, TEXT("AnisotropyBuffer1"));
		AnisotropyBuffer2.Bind(Initializer.ParameterMap, TEXT("AnisotropyBuffer2"));
		AnisotropyBuffer3.Bind(Initializer.ParameterMap, TEXT("AnisotropyBuffer3"));
	}

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType) { return true; }

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << ParticleSizeScale;
		Ar << AnisotropyBuffer1;
		Ar << AnisotropyBuffer2;
		Ar << AnisotropyBuffer3;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		ESceneRenderTargetsMode::Type TextureMode,
		float ParticleScale
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, InMaterialResource, View, View.ViewUniformBuffer, TextureMode);

		if (ParticleSizeScale.IsBound())
		{
			float ParticleSizeScaleValue = ParticleScale;
			SetShaderValue(RHICmdList, GetVertexShader(), ParticleSizeScale, ParticleSizeScaleValue);
		}

		if (AnisotropyBuffer1.IsBound())
		{
			SetSRVParameter(RHICmdList, GetVertexShader(), AnisotropyBuffer1, GAnisotropyResources.AnisoBuffer1.SRV);
		}

		if (AnisotropyBuffer2.IsBound())
		{
			SetSRVParameter(RHICmdList, GetVertexShader(), AnisotropyBuffer2, GAnisotropyResources.AnisoBuffer2.SRV);
		}

		if (AnisotropyBuffer3.IsBound())
		{
			SetSRVParameter(RHICmdList, GetVertexShader(), AnisotropyBuffer3, GAnisotropyResources.AnisoBuffer3.SRV);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}

	FShaderParameter ParticleSizeScale;
	FShaderResourceParameter AnisotropyBuffer1;
	FShaderResourceParameter AnisotropyBuffer2;
	FShaderResourceParameter AnisotropyBuffer3;
};

/*=============================================================================
FFlexFluidSurfaceSpriteSphereVS
=============================================================================*/

class FFlexFluidSurfaceSpriteSphereVS : public FFlexFluidSurfaceSpriteBaseVS
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceSpriteSphereVS, MeshMaterial);
	FFlexFluidSurfaceSpriteSphereVS() {}

public:
	FFlexFluidSurfaceSpriteSphereVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFlexFluidSurfaceSpriteBaseVS(Initializer) {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FFlexFluidSurfaceSpriteSphereVS, TEXT("/Engine/Private/FlexFluidSurfaceSpriteVertexShader.usf"), TEXT("SphereMainVS"), SF_Vertex);

/*=============================================================================
FFlexFluidSurfaceSpriteEllipsoidVS
=============================================================================*/

class FFlexFluidSurfaceSpriteEllipsoidVS : public FFlexFluidSurfaceSpriteBaseVS
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceSpriteEllipsoidVS, MeshMaterial);
	FFlexFluidSurfaceSpriteEllipsoidVS() {}

public:
	FFlexFluidSurfaceSpriteEllipsoidVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFlexFluidSurfaceSpriteBaseVS(Initializer) {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FFlexFluidSurfaceSpriteEllipsoidVS, TEXT("/Engine/Private/FlexFluidSurfaceSpriteVertexShader.usf"), TEXT("EllipsoidMainVS"), SF_Vertex);

/*=============================================================================
FFlexFluidSurfaceSpriteBasePS
=============================================================================*/

class FFlexFluidSurfaceSpriteBasePS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceSpriteBasePS, MeshMaterial);
	FFlexFluidSurfaceSpriteBasePS() {}

public:
	FFlexFluidSurfaceSpriteBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		TexResScaleShader.Bind(Initializer.ParameterMap, TEXT("TexResScale"));
		ParticleSizeScaleInv.Bind(Initializer.ParameterMap, TEXT("ParticleSizeScaleInv"));
	}

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType) { return true; }

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << TexResScaleShader;
		Ar << ParticleSizeScaleInv;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& InMaterialResource,
		const FSceneView& View, ESceneRenderTargetsMode::Type TextureMode, float ParticleScale, float TexResScale)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, InMaterialResource, View, View.ViewUniformBuffer, TextureMode);

		if (ParticleSizeScaleInv.IsBound())
		{
			float ParticleSizeScaleInvValue = 1.0f / ParticleScale;
			SetShaderValue(RHICmdList, GetPixelShader(), ParticleSizeScaleInv, ParticleSizeScaleInvValue);
		}

		if (TexResScaleShader.IsBound())
		{
			SetShaderValue(RHICmdList, GetPixelShader(), TexResScaleShader, TexResScale);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}

	FShaderParameter TexResScaleShader;
	FShaderParameter ParticleSizeScaleInv;
};

/*=============================================================================
FFlexFluidSurfaceSpriteSphereDepthPS
=============================================================================*/

class FFlexFluidSurfaceSpriteSphereDepthPS : public FFlexFluidSurfaceSpriteBasePS
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceSpriteSphereDepthPS, MeshMaterial);
	FFlexFluidSurfaceSpriteSphereDepthPS() {}

public:
	FFlexFluidSurfaceSpriteSphereDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFlexFluidSurfaceSpriteBasePS(Initializer) {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FFlexFluidSurfaceSpriteSphereDepthPS, TEXT("/Engine/Private/FlexFluidSurfaceSpritePixelShader.usf"), TEXT("SphereDepthMainPS"), SF_Pixel);

/*=============================================================================
FFlexFluidSurfaceSpriteEllipsoidDepthPS
=============================================================================*/

class FFlexFluidSurfaceSpriteEllipsoidDepthPS : public FFlexFluidSurfaceSpriteBasePS
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceSpriteEllipsoidDepthPS, MeshMaterial);
	FFlexFluidSurfaceSpriteEllipsoidDepthPS() {}

public:
	FFlexFluidSurfaceSpriteEllipsoidDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFlexFluidSurfaceSpriteBasePS(Initializer) {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FFlexFluidSurfaceSpriteEllipsoidDepthPS, TEXT("/Engine/Private/FlexFluidSurfaceSpritePixelShader.usf"), TEXT("EllipsoidDepthMainPS"), SF_Pixel);

/*=============================================================================
FFlexFluidSurfaceSpriteSphereThicknessPS
=============================================================================*/

class FFlexFluidSurfaceSpriteSphereThicknessPS : public FFlexFluidSurfaceSpriteBasePS
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceSpriteSphereThicknessPS, MeshMaterial);
	FFlexFluidSurfaceSpriteSphereThicknessPS() {}

public:
	FFlexFluidSurfaceSpriteSphereThicknessPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFlexFluidSurfaceSpriteBasePS(Initializer) {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FFlexFluidSurfaceSpriteSphereThicknessPS, TEXT("/Engine/Private/FlexFluidSurfaceSpritePixelShader.usf"), TEXT("SphereThicknessMainPS"), SF_Pixel);

/*=============================================================================
FFlexFluidSurfaceScreenVS
=============================================================================*/

class FFlexFluidSurfaceScreenVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceScreenVS, Global);
	FFlexFluidSurfaceScreenVS() {}

public:
	FFlexFluidSurfaceScreenVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), View.ViewUniformBuffer);
	}
};

IMPLEMENT_SHADER_TYPE(, FFlexFluidSurfaceScreenVS, TEXT("/Engine/Private/FlexFluidSurfaceScreenShader.usf"), TEXT("ScreenMainVS"), SF_Vertex);

/*=============================================================================
FFlexFluidSurfaceDepthSmoothPS
=============================================================================*/

class FFlexFluidSurfaceDepthSmoothPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFlexFluidSurfaceDepthSmoothPS, Global);
	FFlexFluidSurfaceDepthSmoothPS() {}

public:
	FFlexFluidSurfaceDepthSmoothPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DepthTexture.Bind(Initializer.ParameterMap, TEXT("FlexFluidSurfaceDepthTexture"), SPF_Mandatory);
		DepthTextureSampler.Bind(Initializer.ParameterMap, TEXT("FlexFluidSurfaceDepthTextureSampler"));
		SmoothScale.Bind(Initializer.ParameterMap, TEXT("SmoothScale"));
		MaxSmoothTexelRadius.Bind(Initializer.ParameterMap, TEXT("MaxSmoothTexelRadius"));
		DepthEdgeFalloff.Bind(Initializer.ParameterMap, TEXT("DepthEdgeFalloff"));
		TexelSize.Bind(Initializer.ParameterMap, TEXT("TexelSize"));
	}

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_R32_FLOAT);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DepthTexture;
		Ar << DepthTextureSampler;
		Ar << SmoothScale;
		Ar << MaxSmoothTexelRadius;
		Ar << DepthEdgeFalloff;
		Ar << TexelSize;

		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FFlexFluidSurfaceTextures& Textures, float SmoothingRadius, float MaxRadialSmoothingSamples, float SmothingDepthEdgeFalloff, float TexResScale)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

		if (DepthTexture.IsBound())
		{
			FTextureRHIParamRef TextureRHI = GetTexture(Textures.Depth);

			SetTextureParameter(RHICmdList, GetPixelShader(), DepthTexture, DepthTextureSampler,
				TStaticSamplerState<SF_Point, AM_Border, AM_Border, AM_Clamp>::GetRHI(), TextureRHI);
		}

		float FOV = PI / 4.0f;
		float AspectRatio = 1.0f;

		if (View.IsPerspectiveProjection())
		{
			// Derive FOV and aspect ratio from the perspective projection matrix
			FOV = FMath::Atan(1.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0]);
			AspectRatio = View.ViewMatrices.GetProjectionMatrix().M[1][1] / View.ViewMatrices.GetProjectionMatrix().M[0][0];
		}

		if (SmoothScale.IsBound())
		{
			// SmoothScale is the factor used to compute the texture space smoothing radius (R[tex])
			// from the world space surface depth (depth[world]) in the smoothing shader like this:
			// R[tex] = SmoothScale / depth[world]
			// 
			// Derivation:
			// R[tex] / textureHeight == R[world] / h[world](depth[world])
			// h[world](depth[world])*0.5 / depth[world] == tan(FOV*0.5)
			// --> SmoothScale == R[world]*textureHeight*0.5 / tan(FOV*0.5)
			float SmoothScaleValue = SmoothingRadius*View.ViewRect.Height()*0.5f * TexResScale / FMath::Tan(FOV*0.5f);
			SetShaderValue(RHICmdList, GetPixelShader(), SmoothScale, SmoothScaleValue);
		}

		if (MaxSmoothTexelRadius.IsBound())
		{
			SetShaderValue(RHICmdList, GetPixelShader(), MaxSmoothTexelRadius, MaxRadialSmoothingSamples);
		}

		if (DepthEdgeFalloff.IsBound())
		{
			SetShaderValue(RHICmdList, GetPixelShader(), DepthEdgeFalloff, SmothingDepthEdgeFalloff);
		}

		if (TexelSize.IsBound())
		{
			const FIntPoint BufferSize = Textures.BufferSize; // down sampled size in half res.
			FVector2D TexelSizeVal = FVector2D(1.0f / BufferSize.X, 1.0f / BufferSize.Y);
			SetShaderValue(RHICmdList, GetPixelShader(), TexelSize, TexelSizeVal);
		}
	}

	FShaderResourceParameter DepthTexture;
	FShaderResourceParameter DepthTextureSampler;
	FShaderParameter SmoothScale;
	FShaderParameter MaxSmoothTexelRadius;
	FShaderParameter DepthEdgeFalloff;
	FShaderParameter TexelSize;
};

IMPLEMENT_SHADER_TYPE(, FFlexFluidSurfaceDepthSmoothPS, TEXT("/Engine/Private/FlexFluidSurfaceScreenShader.usf"), TEXT("DepthSmoothMainPS"), SF_Pixel);

/** A simple pixel shader used on PC to read scene depth from scene color alpha and write it to a downsized depth buffer. */
class FFlexDownsampleSceneDepthPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFlexDownsampleSceneDepthPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FFlexDownsampleSceneDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		ProjectionScaleBias.Bind(Initializer.ParameterMap, TEXT("ProjectionScaleBias"));
		SourceTexelOffsets01.Bind(Initializer.ParameterMap, TEXT("SourceTexelOffsets01"));
		SourceTexelOffsets23.Bind(Initializer.ParameterMap, TEXT("SourceTexelOffsets23"));
		MinMaxBlend.Bind(Initializer.ParameterMap, TEXT("MinMaxBlend"));
	}
	FFlexDownsampleSceneDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FIntPoint DownsampledBufferSize)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// Used to remap view space Z (which is stored in scene color alpha) into post projection z and w so we can write z/w into the downsized depth buffer
		const FVector2D ProjectionScaleBiasValue(View.ViewMatrices.GetProjectionMatrix().M[2][2], View.ViewMatrices.GetProjectionMatrix().M[3][2]);
		SetShaderValue(RHICmdList, GetPixelShader(), ProjectionScaleBias, ProjectionScaleBiasValue);

		// Offsets of the four full resolution pixels corresponding with a low resolution pixel
		const FVector4 Offsets01(0.0f, 0.0f, 1.0f / DownsampledBufferSize.X, 0.0f);
		SetShaderValue(RHICmdList, GetPixelShader(), SourceTexelOffsets01, Offsets01);
		const FVector4 Offsets23(0.0f, 1.0f / DownsampledBufferSize.Y, 1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, GetPixelShader(), SourceTexelOffsets23, Offsets23);
		SetShaderValue(RHICmdList, GetPixelShader(), MinMaxBlend, 0.0f);
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ProjectionScaleBias;
		Ar << SourceTexelOffsets01;
		Ar << SourceTexelOffsets23;
		Ar << MinMaxBlend;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter ProjectionScaleBias;
	FShaderParameter SourceTexelOffsets01;
	FShaderParameter SourceTexelOffsets23;
	FShaderParameter MinMaxBlend;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(, FFlexDownsampleSceneDepthPS, TEXT("/Engine/Private/DownsampleDepthPixelShader.usf"), TEXT("Main"), SF_Pixel);


/** Shader to up-sample surface depth */
class FFlexUpsampleSurfaceDepthPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFlexUpsampleSurfaceDepthPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FFlexUpsampleSurfaceDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		TexResScale.Bind(Initializer.ParameterMap, TEXT("TexResScale"), SPF_Mandatory);
		DepthTexture.Bind(Initializer.ParameterMap, TEXT("DownsampledDepthTex"), SPF_Mandatory);
		DepthTextureSamplerNearest.Bind(Initializer.ParameterMap, TEXT("DownsampledDepthTexSamplerNearest"));
		DepthTextureSamplerBilinear.Bind(Initializer.ParameterMap, TEXT("DownsampledDepthTexSamplerBilinear"));
	}
	FFlexUpsampleSurfaceDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FTextureRHIParamRef& inputTexture, float TexResScaleValue)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		if (DepthTexture.IsBound() && DepthTextureSamplerNearest.IsBound() && DepthTextureSamplerBilinear.IsBound())
		{
			SetTextureParameter(RHICmdList, GetPixelShader(), DepthTexture, DepthTextureSamplerNearest,
				TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), inputTexture);
			SetTextureParameter(RHICmdList, GetPixelShader(), DepthTexture, DepthTextureSamplerBilinear,
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), inputTexture);
		}
		if (TexResScale.IsBound())
		{
			SetShaderValue(RHICmdList, GetPixelShader(), TexResScale, TexResScaleValue);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TexResScale;
		Ar << DepthTexture;
		Ar << DepthTextureSamplerNearest;
		Ar << DepthTextureSamplerBilinear;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter TexResScale;
	FShaderResourceParameter DepthTexture;
	FShaderResourceParameter DepthTextureSamplerNearest;
	FShaderResourceParameter DepthTextureSamplerBilinear;
};

IMPLEMENT_SHADER_TYPE(, FFlexUpsampleSurfaceDepthPS, TEXT("/Engine/Private/FlexFluidSurfaceUpSampleShader.usf"), TEXT("UpSampleMainPS"), SF_Pixel);

/*=============================================================================
FFlexFluidSurfaceDrawingPolicy, draws the surface with a screen space mesh
=============================================================================*/

class FFlexFluidSurfaceDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** Initialization constructor. */
	FFlexFluidSurfaceDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		ERHIFeatureLevel::Type InFeatureLevel,
		ESceneRenderTargetsMode::Type InSceneTextureMode) :
		FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings),
		SceneTextureMode(InSceneTextureMode)
	{
		SphereVS = InMaterialResource.GetShader<FFlexFluidSurfaceSpriteSphereVS>(InVertexFactory->GetType());
		EllipsoidVS = InMaterialResource.GetShader<FFlexFluidSurfaceSpriteEllipsoidVS>(InVertexFactory->GetType());
		SphereDepthPS = InMaterialResource.GetShader<FFlexFluidSurfaceSpriteSphereDepthPS>(InVertexFactory->GetType());
		EllipsoidDepthPS = InMaterialResource.GetShader<FFlexFluidSurfaceSpriteEllipsoidDepthPS>(InVertexFactory->GetType());
		SphereThicknessPS = InMaterialResource.GetShader<FFlexFluidSurfaceSpriteSphereThicknessPS>(InVertexFactory->GetType());
	}

	// FMeshDrawingPolicy interface.
	bool Matches(const FFlexFluidSurfaceDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other).Result() &&
			SphereVS == Other.SphereVS &&
			EllipsoidVS == Other.EllipsoidVS &&
			SphereDepthPS == Other.SphereDepthPS &&
			EllipsoidDepthPS == Other.EllipsoidDepthPS &&
			SphereThicknessPS == Other.SphereThicknessPS &&
			SceneTextureMode == Other.SceneTextureMode;
	}

	FFlexFluidSurfaceSpriteBaseVS* GetVertexShader(bool bThicknessPass, bool bDrawEllipsoids) const
	{
		if (bThicknessPass || !bDrawEllipsoids)
			return SphereVS;

		return EllipsoidVS;
	}

	FFlexFluidSurfaceSpriteBasePS* GetPixelShader(bool bThicknessPass, bool bDrawEllipsoids) const
	{
		if (bThicknessPass)
			return SphereThicknessPS;

		if (bDrawEllipsoids)
			return EllipsoidDepthPS;

		return SphereDepthPS;
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FViewInfo* View, const ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState,
		bool bThicknessPass, bool bDrawEllipsoids, float ParticleSizeScale, float TexResScale) const
	{
		VertexFactory->Set(RHICmdList);

		FFlexFluidSurfaceSpriteBaseVS* BaseVS = GetVertexShader(bThicknessPass, bDrawEllipsoids);
		FFlexFluidSurfaceSpriteBasePS* BasePS = GetPixelShader(bThicknessPass, bDrawEllipsoids);

		BaseVS->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, SceneTextureMode, ParticleSizeScale);
		BasePS->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, SceneTextureMode, ParticleSizeScale, TexResScale);

		FMeshDrawingPolicy::SetSharedState(RHICmdList, DrawRenderState, View, PolicyContext);
	}

	/**
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel, bool bThicknessPass, bool bDrawEllipsoids)
	{
		FFlexFluidSurfaceSpriteBaseVS* BaseVS = GetVertexShader(bThicknessPass, bDrawEllipsoids);
		FFlexFluidSurfaceSpriteBasePS* BasePS = GetPixelShader(bThicknessPass, bDrawEllipsoids);

		return FBoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(),
			BaseVS->GetVertexShader(),
			FHullShaderRHIParamRef(),
			FDomainShaderRHIParamRef(),
			BasePS->GetPixelShader(),
			FGeometryShaderRHIRef());
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const ContextDataType PolicyContext,
		bool bThicknessPass,
		bool bDrawEllipsoids) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

		FFlexFluidSurfaceSpriteBaseVS* BaseVS = GetVertexShader(bThicknessPass, bDrawEllipsoids);
		FFlexFluidSurfaceSpriteBasePS* BasePS = GetPixelShader(bThicknessPass, bDrawEllipsoids);

		BaseVS->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
		BasePS->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);

		FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex,
			DrawRenderState, FMeshDrawingPolicy::ElementDataType(), PolicyContext);
	}

	friend int32 CompareDrawingPolicy(const FFlexFluidSurfaceDrawingPolicy& A, const FFlexFluidSurfaceDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(SphereVS);
		COMPAREDRAWINGPOLICYMEMBERS(EllipsoidVS);
		COMPAREDRAWINGPOLICYMEMBERS(SphereDepthPS);
		COMPAREDRAWINGPOLICYMEMBERS(EllipsoidDepthPS);
		COMPAREDRAWINGPOLICYMEMBERS(SphereThicknessPS);

		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		return 0;
	}

protected:
	FFlexFluidSurfaceSpriteSphereVS* SphereVS;
	FFlexFluidSurfaceSpriteEllipsoidVS* EllipsoidVS;
	FFlexFluidSurfaceSpriteSphereDepthPS* SphereDepthPS;
	FFlexFluidSurfaceSpriteEllipsoidDepthPS* EllipsoidDepthPS;
	FFlexFluidSurfaceSpriteSphereThicknessPS* SphereThicknessPS;
	ESceneRenderTargetsMode::Type SceneTextureMode;
};

/*=============================================================================
FFlexFluidSurfaceDrawingPolicyFactory
=============================================================================*/

class FFlexFluidSurfaceDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType
	{
		ESceneRenderTargetsMode::Type TextureMode;
		float TexResScale;
		float ParticleSizeScale;
		bool bThicknessPass;
		bool bDrawEllipsoids;

		ContextType(ESceneRenderTargetsMode::Type InTextureMode, bool bInThicknessPass, bool bInDrawEllipsoids, float InParticleSizeScale, float InTexResScale)
			: TextureMode(InTextureMode)
			, TexResScale(InTexResScale)
			, ParticleSizeScale(InParticleSizeScale)
			, bThicknessPass(bInThicknessPass)
			, bDrawEllipsoids(bInDrawEllipsoids)
		{}
	};

	static void AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh) {}

	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		FDrawingPolicyRenderState DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		)
	{
		//Draw depths based on particles
		{
			FFlexFluidSurfaceDrawingPolicy DrawingPolicy(
				Mesh.VertexFactory,
				Mesh.MaterialRenderProxy,
				*Mesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()),
				ComputeMeshOverrideSettings(Mesh),
				View.GetFeatureLevel(),
				ESceneRenderTargetsMode::DontSet
				);

			
			//RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel(),
//				DrawingContext.bThicknessPass, DrawingContext.bDrawEllipsoids));

			DrawingPolicy.SetupPipelineState(DrawRenderState, View);
			CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderState, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel(), DrawingContext.bThicknessPass, DrawingContext.bDrawEllipsoids));
			DrawingPolicy.SetSharedState(RHICmdList, &View, FFlexFluidSurfaceDrawingPolicy::ContextDataType(), DrawRenderState,
				DrawingContext.bThicknessPass, DrawingContext.bDrawEllipsoids, DrawingContext.ParticleSizeScale, DrawingContext.TexResScale);

			check(Mesh.Elements.Num() == 1);
			int BatchElementIndex = 0;
			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderState,
				FFlexFluidSurfaceDrawingPolicy::ContextDataType(), DrawingContext.bThicknessPass, DrawingContext.bDrawEllipsoids);

			DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex);
		}

		return true;
	}

	static bool IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy, ERHIFeatureLevel::Type InFeatureLevel)
	{
		return !MaterialRenderProxy;
	}
};

void AllocateTexturesIfNecessary(FRHICommandList& RHICmdList, FFlexFluidSurfaceTextures& Textures, FIntPoint NewBufferSize, FIntPoint SceneBufferSize)
{
	// Allocate textures if current BufferSize doesn't match up with new one
	if (NewBufferSize != Textures.BufferSize)
	{
		Textures.BufferSize = NewBufferSize;
		bool bAllocHalfResTextures = (NewBufferSize != SceneBufferSize);

		// Release old textures
		{
			Textures.Depth.SafeRelease();
			Textures.DepthStencil.SafeRelease();
			Textures.Thickness.SafeRelease();
			Textures.SmoothDepth.SafeRelease();
			Textures.DownSampledSceneDepth.SafeRelease();
			Textures.UpSampledDepth.SafeRelease();
			GRenderTargetPool.FreeUnusedResources();
		}

		// Alloc new textures
		if (Textures.BufferSize.X > 0 && Textures.BufferSize.Y > 0)
		{
			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Textures.BufferSize, PF_R32_FLOAT, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Textures.Depth, TEXT("FlexFluidSurfaceDepth"));
			}

			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Textures.BufferSize, PF_DepthStencil, FClearValueBinding::None, TexCreate_None, TexCreate_DepthStencilTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Textures.DepthStencil, TEXT("FlexFluidSurfaceDepthStencil"));
			}

			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Textures.BufferSize, PF_R32_FLOAT, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Textures.Thickness, TEXT("FlexFluidSurfaceThickness"));
			}

			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Textures.BufferSize, PF_R32_FLOAT, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Textures.SmoothDepth, TEXT("FlexFluidSurfaceSmoothDepth"));
			}

			if (bAllocHalfResTextures)
			{
				{
					FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Textures.BufferSize, PF_DepthStencil, FClearValueBinding::None, TexCreate_None, TexCreate_DepthStencilTargetable, true));
					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Textures.DownSampledSceneDepth, TEXT("FlexFluidSurfaceDownSampledSceneDepth"));
				}

				{
					FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneBufferSize, PF_R32_FLOAT, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Textures.UpSampledDepth, TEXT("FlexFluidSurfaceUpSampledDepth"));
				}
			}

		}
	}
}

void ClearTextures(FRHICommandList& RHICmdList, FFlexFluidSurfaceTextures& Textures, const FViewInfo& View)
{
	//Clear depth buffers
	{
		SetRenderTarget(RHICmdList, GetSurface(Textures.Depth), GetSurface(Textures.DepthStencil), ESimpleRenderTargetMode::EUninitializedColorAndDepth);

		//Clear depth stencil to 0.0: reversed Z depth surface (0=far, 1=near).
		float Depth = 0.0f;
		float ViewDepth = 65536.0f;

		DrawClearQuad(RHICmdList, true, FLinearColor(FVector(ViewDepth)), true, Depth, false, 0);
	}

	//Clear thickness buffer
	{
		SetRenderTarget(RHICmdList, GetSurface(Textures.Thickness), NULL, ESimpleRenderTargetMode::EClearColorExistingDepth);	// BRG - JDM, please check
	}
}

void UpdateAnisotropyBuffers(const FDynamicSpriteEmitterData* EmitterData)
{
	int32 MaxParticleCount = EmitterData->Source.ActiveParticleCount;
	if (EmitterData->Source.MaxDrawCount >= 0 && EmitterData->Source.ActiveParticleCount > EmitterData->Source.MaxDrawCount)
	{
		MaxParticleCount = EmitterData->Source.MaxDrawCount;
	}

	GAnisotropyResources.AllocateFor(MaxParticleCount);

	FVector4* AnisoBuffer1 = (FVector4*)RHILockVertexBuffer(GAnisotropyResources.AnisoBuffer1.Buffer, 0, GAnisotropyResources.AnisoBuffer1.NumBytes, RLM_WriteOnly);
	FVector4* AnisoBuffer2 = (FVector4*)RHILockVertexBuffer(GAnisotropyResources.AnisoBuffer2.Buffer, 0, GAnisotropyResources.AnisoBuffer2.NumBytes, RLM_WriteOnly);
	FVector4* AnisoBuffer3 = (FVector4*)RHILockVertexBuffer(GAnisotropyResources.AnisoBuffer3.Buffer, 0, GAnisotropyResources.AnisoBuffer3.NumBytes, RLM_WriteOnly);

	check(GAnisotropyResources.MaxParticles >= MaxParticleCount);
	for (int32 i = 0; i < MaxParticleCount; i++)
	{
		const uint8* ParticleData = EmitterData->Source.DataContainer.ParticleData;
		int32 ParticleStride = EmitterData->Source.ParticleStride;
		uint32 ParticleIndex = EmitterData->Source.DataContainer.ParticleIndices[i];

		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride*ParticleIndex);
		verify(EmitterData->Source.FlexDataOffset > 0);

		int32 CurrentOffset = EmitterData->Source.FlexDataOffset;
		const uint8* ParticleBase = (const uint8*)&Particle;
		PARTICLE_ELEMENT(int32, FlexParticleIndex);

		PARTICLE_ELEMENT(FVector, Alignment16);

		PARTICLE_ELEMENT(FVector4, FlexAnisotropy1);
		PARTICLE_ELEMENT(FVector4, FlexAnisotropy2);
		PARTICLE_ELEMENT(FVector4, FlexAnisotropy3);

		AnisoBuffer1[i] = FlexAnisotropy1;
		AnisoBuffer2[i] = FlexAnisotropy2;
		AnisoBuffer3[i] = FlexAnisotropy3;
	}
	RHIUnlockVertexBuffer(GAnisotropyResources.AnisoBuffer1.Buffer);
	RHIUnlockVertexBuffer(GAnisotropyResources.AnisoBuffer2.Buffer);
	RHIUnlockVertexBuffer(GAnisotropyResources.AnisoBuffer3.Buffer);
}

/*
void DownSampleSceneDepth(FRHICommandList& RHICmdList, const FTexture2DRHIRef& destTexture, const FIntPoint& DownsampledBufferSize, const FViewInfo& View, float DownSamplingScale)
{
	SCOPED_DRAW_EVENT(RHICmdList, FlexFluidSurfaceDownsampleDepth);
	
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SetRenderTarget(RHICmdList, NULL, destTexture);

	// Set shaders and texture
	TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
	TShaderMapRef<FFlexDownsampleSceneDepthPS> PixelShader(View.ShaderMap);

	extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
	static FGlobalBoundShaderState DownsampleDepthBoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, DownsampleDepthBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());

	PixelShader->SetParameters(RHICmdList, View, DownsampledBufferSize);

	const uint32 DownsampledX = FMath::TruncToInt(View.ViewRect.Min.X * DownSamplingScale);
	const uint32 DownsampledY = FMath::TruncToInt(View.ViewRect.Min.Y * DownSamplingScale);
	const uint32 DownsampledSizeX = FMath::TruncToInt(View.ViewRect.Width() * DownSamplingScale);
	const uint32 DownsampledSizeY = FMath::TruncToInt(View.ViewRect.Height() * DownSamplingScale);

	RHICmdList.SetViewport(DownsampledX, DownsampledY, 0.0f, DownsampledX + DownsampledSizeX, DownsampledY + DownsampledSizeY, 1.0f);

	DrawRectangle(
		RHICmdList,
		0, 0,
		DownsampledSizeX, DownsampledSizeY,
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(DownsampledSizeX, DownsampledSizeY),
		SceneContext.GetBufferSizeXY(),
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);
}
*/
void UpSampleSurfaceDepth(FRHICommandList& RHICmdList, FFlexFluidSurfaceSceneProxy* SurfaceSceneProxy, const FViewInfo& View)
{
	if(SurfaceSceneProxy->TexResScale == 1.0f)
		return;

	SCOPED_DRAW_EVENT(RHICmdList, FlexFluidSurfaceUpSampleSurfaceDepth);

	SetRenderTarget(RHICmdList, GetSurface(SurfaceSceneProxy->Textures->UpSampledDepth), NULL, ESimpleRenderTargetMode::EUninitializedColorAndDepth);

	// Set shaders and texture
	TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
	TShaderMapRef<FFlexUpsampleSurfaceDepthPS> PixelShader(View.ShaderMap);

	extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
	//static FGlobalBoundShaderState UpSampleDepthBoundShaderState;
//	SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, UpSampleDepthBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

//

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = ScreenVertexShader->GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader->GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(RHICmdList, View, GetTexture(SurfaceSceneProxy->Textures->SmoothDepth), SurfaceSceneProxy->TexResScale);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FIntPoint destBufferSize = SceneContext.GetBufferSizeXY();
	FIntPoint srcBufferSize = SceneContext.GetBufferSizeXY();

	DrawRectangle(
		RHICmdList,
		0, 0,
		destBufferSize.X, destBufferSize.Y,
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		destBufferSize,
		srcBufferSize,
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SurfaceSceneProxy->Textures->UpSampledDepth);
}

void RenderParticleDepth(FRHICommandList& RHICmdList, FFlexFluidSurfaceSceneProxy* SurfaceSceneProxy, const FViewInfo& View)
{
	check(SurfaceSceneProxy);
	float TexResScale = SurfaceSceneProxy->TexResScale;
	FIntRect ScaledRect = View.ViewRect.Scale(TexResScale);

	SCOPED_DRAW_EVENT(RHICmdList, FlexFluidSurfaceRenderParticleDepth);
	SetRenderTarget(RHICmdList, GetSurface(SurfaceSceneProxy->Textures->Depth), GetSurface(SurfaceSceneProxy->Textures->DepthStencil), ESimpleRenderTargetMode::EExistingColorAndDepth);

	// Note, this is a reversed Z depth surface, using CF_GreaterEqual.
	RHICmdList.SetViewport(ScaledRect.Min.X, ScaledRect.Min.Y, 0, ScaledRect.Max.X, ScaledRect.Max.Y, 1);
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	FDrawingPolicyRenderState DrawRenderState;
	DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_GreaterEqual>::GetRHI());


	for (int i = 0; i < SurfaceSceneProxy->VisibleParticleMeshes.Num(); i++)
	{
		const FSurfaceParticleMesh& ParticleMesh = SurfaceSceneProxy->VisibleParticleMeshes[i];

		//TODO: remove cast
		FDynamicSpriteEmitterData* SpriteEmitterData = ((FDynamicSpriteEmitterData*)ParticleMesh.DynamicEmitterData);
		bool bHasAnisotropy = SpriteEmitterData->Source.bFlexAnisotropyData;

		if (bHasAnisotropy)
		{
			UpdateAnisotropyBuffers(SpriteEmitterData);
		}

		// Draw screen space surface
		FFlexFluidSurfaceDrawingPolicyFactory::ContextType DrawingContext(ESceneRenderTargetsMode::DontSet, false,
			bHasAnisotropy, SurfaceSceneProxy->DepthParticleScale, SurfaceSceneProxy->TexResScale);

		FFlexFluidSurfaceDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext,
			*ParticleMesh.Mesh, false, true, DrawRenderState, ParticleMesh.PSysSceneProxy, ParticleMesh.Mesh->BatchHitProxyId);
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SurfaceSceneProxy->Textures->Depth);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SurfaceSceneProxy->Textures->DepthStencil);
}

void RenderParticleThickness(FRHICommandList& RHICmdList, FFlexFluidSurfaceSceneProxy* SurfaceSceneProxy, const FViewInfo& View)
{
	check(SurfaceSceneProxy);
	float TexResScale = SurfaceSceneProxy->TexResScale;
	FIntRect ScaledRect = View.ViewRect.Scale(TexResScale);

	//render thickness
	if (SurfaceSceneProxy->ThicknessParticleScale > 0.0f)
	{
		SCOPED_DRAW_EVENT(RHICmdList, FlexFluidSurfaceRenderParticleThickness);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		/*
		// downsample depth buffer
		if (TexResScale != 1.0f)
		{
			DownSampleSceneDepth(RHICmdList, GetSurface(SurfaceSceneProxy->Textures->DownSampledSceneDepth), SurfaceSceneProxy->Textures->BufferSize, View, TexResScale);

			SetRenderTarget(RHICmdList, GetSurface(SurfaceSceneProxy->Textures->Thickness), GetSurface(SurfaceSceneProxy->Textures->DownSampledSceneDepth), ESimpleRenderTargetMode::EExistingColorAndDepth);
		}
		else
		*/
		{
			SetRenderTarget(RHICmdList, GetSurface(SurfaceSceneProxy->Textures->Thickness), SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth);
		}

		FDrawingPolicyRenderState DrawRenderState;
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_GreaterEqual>::GetRHI());


		RHICmdList.SetViewport(ScaledRect.Min.X, ScaledRect.Min.Y, 0, ScaledRect.Max.X, ScaledRect.Max.Y, 1);
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

		// Draw screen space surface. Maybe the right solution is to extend the FBasePassOpaqueDrawingPolicy instead
		FFlexFluidSurfaceDrawingPolicyFactory::ContextType DrawingContext(ESceneRenderTargetsMode::DontSet, true,
			false, SurfaceSceneProxy->ThicknessParticleScale, TexResScale);

		for (int i = 0; i < SurfaceSceneProxy->VisibleParticleMeshes.Num(); i++)
		{
			const FSurfaceParticleMesh& ParticleMesh = SurfaceSceneProxy->VisibleParticleMeshes[i];
			FFlexFluidSurfaceDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext,
				*ParticleMesh.Mesh, false, true, DrawRenderState, ParticleMesh.PSysSceneProxy, ParticleMesh.Mesh->BatchHitProxyId);
		}
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SurfaceSceneProxy->Textures->Thickness);
}

void SmoothDepth(FRHICommandList& RHICmdList, const FViewInfo& View, FFlexFluidSurfaceSceneProxy* SurfaceSceneProxy)
{
	SCOPED_DRAW_EVENT(RHICmdList, FlexFluidSurfaceSmoothDepth);

	check(SurfaceSceneProxy && SurfaceSceneProxy->Textures);

	float TexResScale = SurfaceSceneProxy->TexResScale;
	FIntRect ScaledRect = View.ViewRect.Scale(TexResScale);

	//Clear depth stencil to 0.0: reversed Z depth surface (0=far, 1=near).
	SetRenderTarget(RHICmdList, GetSurface(SurfaceSceneProxy->Textures->SmoothDepth), FTextureRHIRef(), ESimpleRenderTargetMode::EUninitializedColorAndDepth);

	auto ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());

	RHICmdList.SetViewport(ScaledRect.Min.X, ScaledRect.Min.Y, 0, ScaledRect.Max.X, ScaledRect.Max.Y, 1);
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	FShader* VertexShader = NULL;
	FShader* PixelShader = NULL;
	if (SurfaceSceneProxy->MaxRadialSamples == 1 || SurfaceSceneProxy->SmoothingRadius == 0.0f)
	{
		//disable smoothing
		static FGlobalBoundShaderState BoundShaderState;
		TShaderMapRef<FScreenVS> CopyVertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> CopyPixelShader(ShaderMap);
		VertexShader = *CopyVertexShader;
		PixelShader = *CopyPixelShader;

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader->GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader->GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);


		CopyVertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
		CopyPixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), GetTexture(SurfaceSceneProxy->Textures->Depth));
	}
	else
	{
		static FGlobalBoundShaderState BoundShaderState;
		TShaderMapRef<FFlexFluidSurfaceScreenVS> SmoothingVertexShader(ShaderMap);
		TShaderMapRef<FFlexFluidSurfaceDepthSmoothPS> SmoothingPixelShader(ShaderMap);
		VertexShader = *SmoothingVertexShader;
		PixelShader = *SmoothingPixelShader;

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader->GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader->GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);


		SmoothingPixelShader->SetParameters(RHICmdList, View, *SurfaceSceneProxy->Textures,
			SurfaceSceneProxy->SmoothingRadius, SurfaceSceneProxy->MaxRadialSamples, SurfaceSceneProxy->DepthEdgeFalloff, TexResScale);
	}

	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		ScaledRect.Min.X, ScaledRect.Min.Y,
		ScaledRect.Width(), ScaledRect.Height(),
		FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
		SurfaceSceneProxy->Textures->BufferSize,
		VertexShader,
		EDRF_UseTriangleOptimization);

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SurfaceSceneProxy->Textures->SmoothDepth);
}

/*=============================================================================
FFlexFluidSurfaceRenderer
=============================================================================*/

void FFlexFluidSurfaceRenderer::UpdateProxiesAndResources(FRHICommandList& RHICmdList, TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>& DynamicMeshElements, FSceneRenderTargets& SceneContext)
{
	//refresh SurfaceSceneProxies from DynamicMeshElements
	SurfaceSceneProxies.Empty(SurfaceSceneProxies.Num());
	for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];
		if (MeshBatchAndRelevance.PrimitiveSceneProxy->IsFlexFluidSurface())
		{
			SurfaceSceneProxies.Add((FFlexFluidSurfaceSceneProxy*)MeshBatchAndRelevance.PrimitiveSceneProxy);
		}
	}

	//for each FFlexFluidSurfaceSceneProxy, get all corresponding particle system proxies and allocate textures if neccessary
	for (int32 i = 0; i < SurfaceSceneProxies.Num(); i++)
	{
		FFlexFluidSurfaceSceneProxy* Proxy = SurfaceSceneProxies[i];
		if (Proxy && Proxy->SurfaceMaterial)
		{
			Proxy->VisibleParticleMeshes.Empty(Proxy->VisibleParticleMeshes.Num());

			for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];

				FDynamicEmitterDataBase* DynamicEmitterData = NULL;
				for (int32 EmitterIndex = 0; EmitterIndex < Proxy->DynamicEmitterDataArray.Num(); EmitterIndex++)
				{
					FParticleSystemSceneProxy* PSysSceneProxy = Proxy->ParticleSystemSceneProxyArray[EmitterIndex];
					if (MeshBatchAndRelevance.PrimitiveSceneProxy == PSysSceneProxy)
					{
						for (int32 CandidateIndex = 0; CandidateIndex < PSysSceneProxy->GetDynamicData()->DynamicEmitterDataArray.Num(); CandidateIndex++)
						{
							if (PSysSceneProxy->GetDynamicData()->DynamicEmitterDataArray[CandidateIndex] == Proxy->DynamicEmitterDataArray[EmitterIndex])
							{
								DynamicEmitterData = Proxy->DynamicEmitterDataArray[EmitterIndex];
							}
						}
					}
				}

				if (DynamicEmitterData)
				{
					FSurfaceParticleMesh ParticleMesh;
					ParticleMesh.DynamicEmitterData = DynamicEmitterData;
					ParticleMesh.PSysSceneProxy = (FParticleSystemSceneProxy*)MeshBatchAndRelevance.PrimitiveSceneProxy;
					ParticleMesh.Mesh = MeshBatchAndRelevance.Mesh;

					Proxy->VisibleParticleMeshes.Add(ParticleMesh);
				}
			}

			FIntPoint bufferSize = SceneContext.GetBufferSizeXY();
			bufferSize.X = FPlatformMath::CeilToInt(bufferSize.X*Proxy->TexResScale);
			bufferSize.Y = FPlatformMath::CeilToInt(bufferSize.Y*Proxy->TexResScale);
			AllocateTexturesIfNecessary(RHICmdList, *Proxy->Textures, bufferSize, SceneContext.GetBufferSizeXY());
		}
	}

}

void FFlexFluidSurfaceRenderer::RenderParticles(FRHICommandList& RHICmdList, const TArray<FViewInfo>& Views)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 viewIdx = 0; viewIdx < Views.Num(); viewIdx++)
	{
		const FViewInfo& View = Views[viewIdx];

		for (int32 i = 0; i < SurfaceSceneProxies.Num(); i++)
		{
			FFlexFluidSurfaceSceneProxy* Proxy = SurfaceSceneProxies[i];
			if (Proxy && Proxy->SurfaceMaterial)
			{
				if (viewIdx == 0)
				{
					ClearTextures(RHICmdList, *Proxy->Textures, View);
				}
				RenderParticleDepth(RHICmdList, Proxy, View);
				RenderParticleThickness(RHICmdList, Proxy, View);
				SmoothDepth(RHICmdList, View, Proxy);
				UpSampleSurfaceDepth(RHICmdList, Proxy, View);
			}
		}
	}
}

void FFlexFluidSurfaceRenderer::RenderBasePass(FRHICommandList& RHICmdList, const TArray<FViewInfo>& Views)
{
	for (int32 viewIdx = 0; viewIdx < Views.Num(); viewIdx++)
	{
		const FViewInfo& View = Views[viewIdx];

		for (int32 i = 0; i < SurfaceSceneProxies.Num(); i++)
		{
			FFlexFluidSurfaceSceneProxy* Proxy = SurfaceSceneProxies[i];			
			if (Proxy && Proxy->SurfaceMaterial)
			{
				EBlendMode BlendMode = Proxy->SurfaceMaterial->GetBlendMode();

				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					SCOPED_DRAW_EVENT(RHICmdList, FlexFluidSurfaceRenderBasePass);
					//SetupBasePassView in DeferredShadingRenderer.cpp
					{
						RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
						RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
					}

					FDrawingPolicyRenderState DrawRenderState(View);
					//DrawRenderState.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());					
	
					// Opaque blending for all G buffer targets, depth tests and writes.				
					DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());
					// Note, this is a reversed Z depth surface, using CF_GreaterEqual.
					DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_GreaterEqual>::GetRHI());	

					FBasePassOpaqueDrawingPolicyFactory::ContextType DrawingContext(false, ESceneRenderTargetsMode::DontSet);
					FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext, *Proxy->MeshBatch, false, DrawRenderState, Proxy, Proxy->MeshBatch->BatchHitProxyId);
				}
			}
		}
	}
}

bool FFlexFluidSurfaceRenderer::IsDepthMaskingRequired(const FPrimitiveSceneProxy* SceneProxy)
{
	if (SceneProxy->IsFlexFluidSurface())
	{
		return true;
	}

	if (!SceneProxy->IsOftenMoving() || !SceneProxy->CastsDynamicShadow())
	{
		return false;
	}

	for (int32 i = 0; i < SurfaceSceneProxies.Num(); i++)
	{
		FFlexFluidSurfaceSceneProxy* SurfaceProxy = SurfaceSceneProxies[i];
		if (SurfaceProxy && SurfaceProxy->SurfaceMaterial)
		{
			for (int32 v = 0; v < SurfaceProxy->VisibleParticleMeshes.Num(); v++)
			{
				if (SurfaceProxy->VisibleParticleMeshes[v].PSysSceneProxy == SceneProxy)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void FFlexFluidSurfaceRenderer::RenderDepth(FRHICommandList& RHICmdList, FPrimitiveSceneProxy* SceneProxy, const FViewInfo& View)
{
	if (SceneProxy->IsFlexFluidSurface())
	{
		FFlexFluidSurfaceSceneProxy* SurfaceProxy = (FFlexFluidSurfaceSceneProxy*)SceneProxy;
		if (SurfaceProxy && SurfaceProxy->SurfaceMaterial)
		{
			EBlendMode BlendMode = SurfaceProxy->SurfaceMaterial->GetBlendMode();

			//TODO fix shadowing for translucent
			if (BlendMode == BLEND_Translucent)
				return;

			SCOPED_DRAW_EVENT(RHICmdList, FlexFluidSurfaceRenderDepth);

			FDrawingPolicyRenderState DrawRenderState(View);

			FDepthDrawingPolicyFactory::ContextType DrawingContext(DDM_AllOccluders, false);
			FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext, *SurfaceProxy->MeshBatch,
				false, DrawRenderState, SurfaceProxy, SurfaceProxy->MeshBatch->BatchHitProxyId);
		}
	}
}

void FFlexFluidSurfaceRenderer::Cleanup()
{
	SurfaceSceneProxies.Empty(SurfaceSceneProxies.Num());
}
