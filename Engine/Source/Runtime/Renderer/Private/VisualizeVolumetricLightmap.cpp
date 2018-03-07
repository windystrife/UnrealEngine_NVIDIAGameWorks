// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricLightmap.cpp
=============================================================================*/

#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "ScenePrivate.h"
#include "SpriteIndexBuffer.h"
#include "SceneFilterRendering.h"
#include "PrecomputedVolumetricLightmap.h"

float GVolumetricLightmapVisualizationRadiusScale = .01f;
FAutoConsoleVariableRef CVarVolumetricLightmapVisualizationRadiusScale(
	TEXT("r.VolumetricLightmap.VisualizationRadiusScale"),
	GVolumetricLightmapVisualizationRadiusScale,
	TEXT("Scales the size of the spheres used to visualize volumetric lightmap samples."),
	ECVF_RenderThreadSafe
	);

float GVolumetricLightmapVisualizationMinScreenFraction = .001f;
FAutoConsoleVariableRef CVarVolumetricLightmapVisualizationMinScreenFraction(
	TEXT("r.VolumetricLightmap.VisualizationMinScreenFraction"),
	GVolumetricLightmapVisualizationMinScreenFraction,
	TEXT("Minimum screen size of a volumetric lightmap visualization sphere"),
	ECVF_RenderThreadSafe
	);

// Nvidia has lower vertex throughput when only processing a few verts per instance
const int32 GQuadsPerVisualizeInstance = 8;

TGlobalResource< FSpriteIndexBuffer<GQuadsPerVisualizeInstance> > GVisualizeQuadIndexBuffer;

class FVisualizeVolumetricLightmapVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeVolumetricLightmapVS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("QUADS_PER_INSTANCE"), GQuadsPerVisualizeInstance);
	}

	/** Default constructor. */
	FVisualizeVolumetricLightmapVS() {}

	/** Initialization constructor. */
	FVisualizeVolumetricLightmapVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VisualizationRadiusScale.Bind(Initializer.ParameterMap, TEXT("VisualizationRadiusScale"));
		VisualizationMinScreenFraction.Bind(Initializer.ParameterMap, TEXT("VisualizationMinScreenFraction"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetShaderValue(RHICmdList, ShaderRHI, VisualizationRadiusScale, GVolumetricLightmapVisualizationRadiusScale);
		SetShaderValue(RHICmdList, ShaderRHI, VisualizationMinScreenFraction, GVolumetricLightmapVisualizationMinScreenFraction);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VisualizationRadiusScale;
		Ar << VisualizationMinScreenFraction;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter VisualizationRadiusScale;
	FShaderParameter VisualizationMinScreenFraction;
};

IMPLEMENT_SHADER_TYPE(,FVisualizeVolumetricLightmapVS,TEXT("/Engine/Private/VisualizeVolumetricLightmap.usf"),TEXT("VisualizeVolumetricLightmapVS"),SF_Vertex);


class FVisualizeVolumetricLightmapPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeVolumetricLightmapPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{

	}

	/** Default constructor. */
	FVisualizeVolumetricLightmapPS() {}

	/** Initialization constructor. */
	FVisualizeVolumetricLightmapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DiffuseColor.Bind(Initializer.ParameterMap, TEXT("DiffuseColor"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		FLinearColor DiffuseColorValue(.18f, .18f, .18f);

		if (!View.Family->EngineShowFlags.Materials)
		{
			DiffuseColorValue = GEngine->LightingOnlyBrightness;
		}

		SetShaderValue(RHICmdList, ShaderRHI, DiffuseColor, DiffuseColorValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DiffuseColor;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter DiffuseColor;
};

IMPLEMENT_SHADER_TYPE(,FVisualizeVolumetricLightmapPS,TEXT("/Engine/Private/VisualizeVolumetricLightmap.usf"),TEXT("VisualizeVolumetricLightmapPS"),SF_Pixel);

void FDeferredShadingSceneRenderer::VisualizeVolumetricLightmap(FRHICommandListImmediate& RHICmdList)
{
	if (ViewFamily.EngineShowFlags.VisualizeVolumetricLightmap
		&& Scene->VolumetricLightmapSceneData.GetLevelVolumetricLightmap()
		&& Scene->VolumetricLightmapSceneData.GetLevelVolumetricLightmap()->Data->IndirectionTextureDimensions.GetMin() > 0)
	{
		const FPrecomputedVolumetricLightmapData* VolumetricLightmapData = Scene->VolumetricLightmapSceneData.GetLevelVolumetricLightmap()->Data;

		SCOPED_DRAW_EVENT(RHICmdList, VisualizeVolumetricLightmap);
					
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		int32 NumRenderTargets = 1;

		FTextureRHIParamRef RenderTargets[2] =
		{
			SceneContext.GetSceneColorSurface(),
			NULL,
		};

		if (SceneContext.GBufferB)
		{
			RenderTargets[NumRenderTargets] = SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture;
			NumRenderTargets++;
		}

		SetRenderTargets(RHICmdList, NumRenderTargets, RenderTargets, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGB,CW_RGBA>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			TShaderMapRef<FVisualizeVolumetricLightmapVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FVisualizeVolumetricLightmapPS> PixelShader(View.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			VertexShader->SetParameters(RHICmdList, View);
			PixelShader->SetParameters(RHICmdList, View);

			int32 BrickSize = VolumetricLightmapData->BrickSize;
			int32 NumQuads = VolumetricLightmapData->IndirectionTextureDimensions.X * VolumetricLightmapData->IndirectionTextureDimensions.Y * VolumetricLightmapData->IndirectionTextureDimensions.Z * BrickSize * BrickSize * BrickSize;

			RHICmdList.SetStreamSource(0, NULL, 0);
			RHICmdList.DrawIndexedPrimitive(GVisualizeQuadIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 4 * GQuadsPerVisualizeInstance, 0, 2 * GQuadsPerVisualizeInstance, FMath::DivideAndRoundUp(NumQuads, GQuadsPerVisualizeInstance));
		}
	}
}