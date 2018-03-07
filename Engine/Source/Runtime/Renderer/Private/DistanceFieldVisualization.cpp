// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldVisualization.cpp
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

template<bool bUseGlobalDistanceField>
class TVisualizeMeshDistanceFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TVisualizeMeshDistanceFieldCS, Global);
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
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_DISTANCE_FIELD"), bUseGlobalDistanceField);
	}

	/** Default constructor. */
	TVisualizeMeshDistanceFieldCS() {}

	/** Initialization constructor. */
	TVisualizeMeshDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VisualizeMeshDistanceFields.Bind(Initializer.ParameterMap, TEXT("VisualizeMeshDistanceFields"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& VisualizeMeshDistanceFieldsValue, 
		FVector2D NumGroupsValue,
		const FDistanceFieldAOParameters& Parameters,
		const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, VisualizeMeshDistanceFieldsValue.UAV);
		VisualizeMeshDistanceFields.SetTexture(RHICmdList, ShaderRHI, VisualizeMeshDistanceFieldsValue.ShaderResourceTexture, VisualizeMeshDistanceFieldsValue.UAV);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		if (bUseGlobalDistanceField)
		{
			GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, GlobalDistanceFieldInfo.ParameterData);
		}

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& VisualizeMeshDistanceFieldsValue)
	{
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, VisualizeMeshDistanceFieldsValue.UAV);
		VisualizeMeshDistanceFields.UnsetUAV(RHICmdList, GetComputeShader());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VisualizeMeshDistanceFields;
		Ar << NumGroups;
		Ar << ObjectParameters;
		Ar << DeferredParameters;
		Ar << AOParameters;
		Ar << GlobalDistanceFieldParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter VisualizeMeshDistanceFields;
	FShaderParameter NumGroups;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TVisualizeMeshDistanceFieldCS<true>,TEXT("/Engine/Private/DistanceFieldVisualization.usf"),TEXT("VisualizeMeshDistanceFieldCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TVisualizeMeshDistanceFieldCS<false>,TEXT("/Engine/Private/DistanceFieldVisualization.usf"),TEXT("VisualizeMeshDistanceFieldCS"),SF_Compute);

class FVisualizeDistanceFieldUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeDistanceFieldUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
	}

	/** Default constructor. */
	FVisualizeDistanceFieldUpsamplePS() {}

	/** Initialization constructor. */
	FVisualizeDistanceFieldUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		VisualizeDistanceFieldTexture.Bind(Initializer.ParameterMap,TEXT("VisualizeDistanceFieldTexture"));
		VisualizeDistanceFieldSampler.Bind(Initializer.ParameterMap,TEXT("VisualizeDistanceFieldSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget>& VisualizeDistanceField)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		SetTextureParameter(RHICmdList, ShaderRHI, VisualizeDistanceFieldTexture, VisualizeDistanceFieldSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), VisualizeDistanceField->GetRenderTargetItem().ShaderResourceTexture);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << VisualizeDistanceFieldTexture;
		Ar << VisualizeDistanceFieldSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter VisualizeDistanceFieldTexture;
	FShaderResourceParameter VisualizeDistanceFieldSampler;
};

IMPLEMENT_SHADER_TYPE(,FVisualizeDistanceFieldUpsamplePS,TEXT("/Engine/Private/DistanceFieldVisualization.usf"),TEXT("VisualizeDistanceFieldUpsamplePS"),SF_Pixel);


void FDeferredShadingSceneRenderer::RenderMeshDistanceFieldVisualization(FRHICommandListImmediate& RHICmdList, const FDistanceFieldAOParameters& Parameters)
{
	//@todo - support multiple views
	const FViewInfo& View = Views[0];

	extern int32 GDistanceFieldAO;

	if (GDistanceFieldAO 
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldAO(View.GetShaderPlatform())
		&& Views.Num() == 1)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderMeshDistanceFieldVis);
		SCOPED_DRAW_EVENT(RHICmdList, VisualizeMeshDistanceFields);

		if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0)
		{
			check(!Scene->DistanceFieldSceneData.HasPendingOperations());

			QUICK_SCOPE_CYCLE_COUNTER(STAT_AOIssueGPUWork);

			const bool bUseGlobalDistanceField = UseGlobalDistanceField(Parameters) && View.Family->EngineShowFlags.VisualizeGlobalDistanceField;

			CullObjectsToView(RHICmdList, Scene, View, Parameters, GAOCulledObjectBuffers);

			TRefCountPtr<IPooledRenderTarget> VisualizeResultRT;

			{
				const FIntPoint BufferSize = GetBufferSizeForAO();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VisualizeResultRT, TEXT("VisualizeDistanceField"));
			}

			{
				SetRenderTarget(RHICmdList, NULL, NULL);

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& ViewInfo = Views[ViewIndex];

					uint32 GroupSizeX = FMath::DivideAndRoundUp(ViewInfo.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX);
					uint32 GroupSizeY = FMath::DivideAndRoundUp(ViewInfo.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY);

					SCOPED_DRAW_EVENT(RHICmdList, VisualizeMeshDistanceFieldCS);

					FSceneRenderTargetItem& VisualizeResultRTI = VisualizeResultRT->GetRenderTargetItem();
					if (bUseGlobalDistanceField)
					{
						check(View.GlobalDistanceFieldInfo.Clipmaps.Num() > 0);

						TShaderMapRef<TVisualizeMeshDistanceFieldCS<true> > ComputeShader(ViewInfo.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, ViewInfo, VisualizeResultRTI, FVector2D(GroupSizeX, GroupSizeY), Parameters, View.GlobalDistanceFieldInfo);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

						ComputeShader->UnsetParameters(RHICmdList, VisualizeResultRTI);
					}
					else
					{
						TShaderMapRef<TVisualizeMeshDistanceFieldCS<false> > ComputeShader(ViewInfo.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, ViewInfo, VisualizeResultRTI, FVector2D(GroupSizeX, GroupSizeY), Parameters, View.GlobalDistanceFieldInfo);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

						ComputeShader->UnsetParameters(RHICmdList, VisualizeResultRTI);
					}
				}
			}

			if ( IsTransientResourceBufferAliasingEnabled())
			{
				GAOCulledObjectBuffers.Buffers.DiscardTransientResource();
			}

			{
				FSceneRenderTargets::Get(RHICmdList).BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilRead);

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& ViewInfo = Views[ViewIndex];

					SCOPED_DRAW_EVENT(RHICmdList, UpsampleAO);

					RHICmdList.SetViewport(ViewInfo.ViewRect.Min.X, ViewInfo.ViewRect.Min.Y, 0.0f, ViewInfo.ViewRect.Max.X, ViewInfo.ViewRect.Max.Y, 1.0f);
					
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

					TShaderMapRef<FPostProcessVS> VertexShader( ViewInfo.ShaderMap );
					TShaderMapRef<FVisualizeDistanceFieldUpsamplePS> PixelShader( ViewInfo.ShaderMap );

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, ViewInfo, VisualizeResultRT);

					DrawRectangle( 
						RHICmdList,
						0, 0, 
						ViewInfo.ViewRect.Width(), ViewInfo.ViewRect.Height(),
						ViewInfo.ViewRect.Min.X / GAODownsampleFactor, ViewInfo.ViewRect.Min.Y / GAODownsampleFactor, 
						ViewInfo.ViewRect.Width() / GAODownsampleFactor, ViewInfo.ViewRect.Height() / GAODownsampleFactor,
						FIntPoint(ViewInfo.ViewRect.Width(), ViewInfo.ViewRect.Height()),
						GetBufferSizeForAO(),
						*VertexShader);
				}
			}
		}
	}
}
