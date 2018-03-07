// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessSelectionOutline.cpp: Post processing outline effect.
=============================================================================*/

#include "PostProcess/PostProcessSelectionOutline.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "SceneHitProxyRendering.h"
#include "ScenePrivate.h"
#include "EngineGlobals.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

#if WITH_EDITOR

#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"

///////////////////////////////////////////
// FRCPassPostProcessSelectionOutlineColor
///////////////////////////////////////////

void FRCPassPostProcessSelectionOutlineColor::Process(FRenderingCompositePassContext& Context)
{
#if WITH_EDITOR
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSelectionOutlineBuffer);

	const FPooledRenderTargetDesc* SceneColorInputDesc = GetInputDesc(ePId_Input0);

	if(!SceneColorInputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	FIntRect ViewRect = View.ViewRect;
	FIntPoint SrcSize = SceneColorInputDesc->Extent;

	FDrawingPolicyRenderState DrawRenderState(View);

	// Get the output render target
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// Set the render target/viewport.
	FRHIDepthRenderTargetView DepthRT(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);
	FRHISetRenderTargetsInfo RTInfo(0, nullptr, DepthRT);

	if (GRHIRequiresRenderTargetForPixelShaderUAVs)
	{
		FIntVector ss = PassOutputs[0].RenderTargetDesc.GetSize();

		TRefCountPtr<IPooledRenderTarget> Dummy;
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(ss.X, ss.Y), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, Dummy, TEXT("Dummy"));
		FRHIRenderTargetView DummyRTView(Dummy->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ENoAction);

		RTInfo = FRHISetRenderTargetsInfo(1, &DummyRTView, DepthRT);
	}

	Context.RHICmdList.SetRenderTargetsAndClear(RTInfo);

	Context.SetViewportAndCallRHI(ViewRect);

	WaitForInputPassComputeFences(Context.RHICmdList);

	if (View.Family->EngineShowFlags.Selection)
	{
		FHitProxyDrawingPolicyFactory::ContextType FactoryContext;
		DrawRenderState.ModifyViewOverrideFlags() |= EDrawingPolicyOverrideFlags::TwoSided;
		DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI());
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		// Note that the stencil value will overflow with enough selected objects
		FEditorSelectionDrawingPolicy::ResetStencilValues();

		// Run selection pass on static elements
		FScene* Scene = View.Family->Scene->GetRenderScene();
		if(Scene)
		{	
			Scene->EditorSelectionDrawList.DrawVisible(Context.RHICmdList, View, DrawRenderState, View.StaticMeshEditorSelectionMap, View.StaticMeshBatchVisibility);
		}
	
		for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = MeshBatchAndRelevance.PrimitiveSceneProxy;

			// Selected actors should be subdued if any component is individually selected
			bool bActorSelectionColorIsSubdued = View.bHasSelectedComponents;

			if (PrimitiveSceneProxy->IsSelected() && MeshBatchAndRelevance.Mesh->bUseSelectionOutline && PrimitiveSceneProxy->WantsSelectionOutline())
			{
				int32 StencilValue = 1;
				if(PrimitiveSceneProxy->GetOwnerName() != NAME_BSP)
				{
					StencilValue = FEditorSelectionDrawingPolicy::GetStencilValue(View, PrimitiveSceneProxy);

				}

				// Note that the stencil value will overflow with enough selected objects
				DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI());
				DrawRenderState.SetStencilRef(StencilValue);

				const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
				FHitProxyDrawingPolicyFactory::DrawDynamicMesh(Context.RHICmdList, View, FactoryContext, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
			}
		}

		// @third party code - BEGIN HairWorks
		HairWorksRenderer::RenderSelectionOutline(Context.RHICmdList, View);
		// @third party code - END HairWorks

		// to get an outline around the objects if it's partly outside of the screen
		{
			FIntRect InnerRect = ViewRect;

			// 1 as we have an outline that is that thick
			InnerRect.InflateRect(-1);

			// We could use Clear with InnerRect but this is just an optimization - on some hardware it might do a full clear (and we cannot disable yet)
			//			RHICmdList.Clear(false, FLinearColor(0, 0, 0, 0), true, 0.0f, true, 0, InnerRect);
			// so we to 4 clears - one for each border.

			// top
			Context.RHICmdList.SetScissorRect(true, ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, InnerRect.Min.Y);
			DrawClearQuad(Context.RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, PassOutputs[0].RenderTargetDesc.Extent, FIntRect());
			// bottom
			Context.RHICmdList.SetScissorRect(true, ViewRect.Min.X, InnerRect.Max.Y, ViewRect.Max.X, ViewRect.Max.Y);
			DrawClearQuad(Context.RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, PassOutputs[0].RenderTargetDesc.Extent, FIntRect());
			// left
			Context.RHICmdList.SetScissorRect(true, ViewRect.Min.X, ViewRect.Min.Y, InnerRect.Min.X, ViewRect.Max.Y);
			DrawClearQuad(Context.RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, PassOutputs[0].RenderTargetDesc.Extent, FIntRect());
			// right
			Context.RHICmdList.SetScissorRect(true, InnerRect.Max.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);
			DrawClearQuad(Context.RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, PassOutputs[0].RenderTargetDesc.Extent, FIntRect());

			Context.RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		}
	}
	
	// Resolve to the output
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
#endif	
}

FPooledRenderTargetDesc FRCPassPostProcessSelectionOutlineColor::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();

	Ret.Format = PF_DepthStencil;
	Ret.Flags = TexCreate_None;
	Ret.ClearValue = FClearValueBinding::DepthFar;

	//mark targetable as TexCreate_ShaderResource because we actually do want to sample from the unresolved MSAA target in this case.
	Ret.TargetableFlags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
	Ret.DebugName = TEXT("SelectionDepthStencil");
	Ret.NumSamples = FSceneRenderTargets::Get_FrameConstantsOnly().GetEditorMSAACompositingSampleCount();

	// This is a reversed Z depth surface, so 0.0f is the far plane.
	Ret.ClearValue = FClearValueBinding((float)ERHIZBuffer::FarPlane, 0);

	return Ret;
}


///////////////////////////////////////////
// FRCPassPostProcessSelectionOutline
///////////////////////////////////////////

/**
 * Pixel shader for rendering the selection outline
 */
template<uint32 MSAASampleCount>
class FPostProcessSelectionOutlinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSelectionOutlinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		if(!IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5))
		{
			if(MSAASampleCount > 1)
			{
				return false;
			}
		}

		return IsPCPlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("MSAA_SAMPLE_COUNT"), MSAASampleCount);
	}

	/** Default constructor. */
	FPostProcessSelectionOutlinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter OutlineColor;
	FShaderParameter SubduedOutlineColor;
	FShaderParameter BSPSelectionIntensity;
	FShaderResourceParameter PostprocessInput1MS;
	FShaderResourceParameter EditorPrimitivesStencil;
	FShaderParameter EditorRenderParams;

	/** Initialization constructor. */
	FPostProcessSelectionOutlinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		OutlineColor.Bind(Initializer.ParameterMap, TEXT("OutlineColor"));
		SubduedOutlineColor.Bind(Initializer.ParameterMap, TEXT("SubduedOutlineColor"));
		BSPSelectionIntensity.Bind(Initializer.ParameterMap, TEXT("BSPSelectionIntensity"));
		PostprocessInput1MS.Bind(Initializer.ParameterMap, TEXT("PostprocessInput1MS"));
		EditorRenderParams.Bind(Initializer.ParameterMap, TEXT("EditorRenderParams"));
		EditorPrimitivesStencil.Bind(Initializer.ParameterMap,TEXT("EditorPrimitivesStencil"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		// PostprocessInput1MS and EditorPrimitivesStencil
		{
			FRenderingCompositeOutputRef* OutputRef = Context.Pass->GetInput(ePId_Input1);

			check(OutputRef);

			FRenderingCompositeOutput* Input = OutputRef->GetOutput();

			check(Input);

			TRefCountPtr<IPooledRenderTarget> InputPooledElement = Input->RequestInput();

			check(InputPooledElement);

			FTexture2DRHIRef& TargetableTexture = (FTexture2DRHIRef&)InputPooledElement->GetRenderTargetItem().TargetableTexture;

			SetTextureParameter(Context.RHICmdList, ShaderRHI, PostprocessInput1MS, TargetableTexture);

			if(EditorPrimitivesStencil.IsBound())
			{
				// cache the stencil SRV to avoid create calls each frame (the cache element is stored in the state)
				if(ViewState->SelectionOutlineCacheKey != TargetableTexture)
				{
					// release if not the right one (as the internally SRV stores a pointer to the texture we cannot get a false positive)
					ViewState->SelectionOutlineCacheKey.SafeRelease();
					ViewState->SelectionOutlineCacheValue.SafeRelease();
				}

				if(!ViewState->SelectionOutlineCacheValue)
				{
					// create if needed
					ViewState->SelectionOutlineCacheKey = TargetableTexture;
					ViewState->SelectionOutlineCacheValue = RHICreateShaderResourceView(TargetableTexture, 0, 1, PF_X24_G8);
				}

				SetSRVParameter(Context.RHICmdList, ShaderRHI, EditorPrimitivesStencil, ViewState->SelectionOutlineCacheValue);
			}
		}

#if WITH_EDITOR
		{
			FLinearColor OutlineColorValue = Context.View.SelectionOutlineColor;
			FLinearColor SubduedOutlineColorValue = Context.View.SubduedSelectionOutlineColor;
			OutlineColorValue.A = GEngine->SelectionHighlightIntensity;

			SetShaderValue(Context.RHICmdList, ShaderRHI, OutlineColor, OutlineColorValue);
			SetShaderValue(Context.RHICmdList, ShaderRHI, SubduedOutlineColor, SubduedOutlineColorValue);
			SetShaderValue(Context.RHICmdList, ShaderRHI, BSPSelectionIntensity, GEngine->BSPSelectionHighlightIntensity);
		}
#else
		check(!"This shader is not used outside of the Editor.");
#endif

		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Editor.MovingPattern"));
		
			FLinearColor Value(0, CVar->GetValueOnRenderThread(), 0, 0);

			if(!ViewFamily.bRealtimeUpdate)
			{
				// no animation if realtime update is disabled
				Value.G = 0;
			}

			SetShaderValue(Context.RHICmdList, ShaderRHI, EditorRenderParams, Value);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << OutlineColor << SubduedOutlineColor << BSPSelectionIntensity << DeferredParameters << PostprocessInput1MS << EditorPrimitivesStencil << EditorRenderParams;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessSelectionOutline.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessSelectionOutlinePS<A> FPostProcessSelectionOutlinePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessSelectionOutlinePS##A, SF_Pixel);

VARIATION1(1)			VARIATION1(2)			VARIATION1(4)			VARIATION1(8)
#undef VARIATION1


template <uint32 MSAASampleCount>
static void SetSelectionOutlineShaderTempl(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSelectionOutlinePS<MSAASampleCount> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSelectionOutline::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSelectionOutline);
	const FPooledRenderTargetDesc* SceneColorInputDesc = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* SelectionColorInputDesc = GetInputDesc(ePId_Input1);

	if (!SceneColorInputDesc || !SelectionColorInputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	FIntRect ViewRect = View.ViewRect;
	FIntPoint SrcSize = SceneColorInputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(ViewRect);

	const uint32 MSAASampleCount = FSceneRenderTargets::Get(Context.RHICmdList).GetEditorMSAACompositingSampleCount();

	if(MSAASampleCount == 1)
	{
		SetSelectionOutlineShaderTempl<1>(Context);
	}
	else if(MSAASampleCount == 2)
	{
		SetSelectionOutlineShaderTempl<2>(Context);
	}
	else if(MSAASampleCount == 4)
	{
		SetSelectionOutlineShaderTempl<4>(Context);
	}
	else if(MSAASampleCount == 8)
	{
		SetSelectionOutlineShaderTempl<8>(Context);
	}
	else
	{
		// not supported, internal error
		check(0);
	}

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		ViewRect.Width(), ViewRect.Height(),
		ViewRect.Min.X, ViewRect.Min.Y,
		ViewRect.Width(), ViewRect.Height(),
		ViewRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSelectionOutline::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Reset();
	Ret.DebugName = TEXT("SelectionComposited");

	return Ret;
}

#endif
