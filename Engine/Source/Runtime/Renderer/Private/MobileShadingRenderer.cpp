// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileShadingRenderer.cpp: Scene rendering code for the ES2 feature level.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "EngineGlobals.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "SceneUtils.h"
#include "UniformBuffer.h"
#include "Engine/BlendableInterface.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "FXSystem.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMobile.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/PostProcessCompositeEditorPrimitives.h"
#include "PostProcess/PostProcessHMD.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "SceneViewExtension.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "MobileSeparateTranslucencyPass.h"


uint32 GetShadowQuality();

static TAutoConsoleVariable<int32> CVarMobileAlwaysResolveDepth(
	TEXT("r.Mobile.AlwaysResolveDepth"),
	0,
	TEXT("0: Depth buffer is resolved after opaque pass only when decals or modulated shadows are in use. (Default)\n")
	TEXT("1: Depth buffer is always resolved after opaque pass.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileForceDepthResolve(
	TEXT("r.Mobile.ForceDepthResolve"),
	0,
	TEXT("0: Depth buffer is resolved by switching out render targets. (Default)\n")
	TEXT("1: Depth buffer is resolved by switching out render targets and drawing with the depth texture.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

FMobileSceneRenderer::FMobileSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer)
	:	FSceneRenderer(InViewFamily, HitProxyConsumer)
{
	bModulatedShadowsInUse = false;
	bPostProcessUsesDepthTexture = false;
}

TUniformBufferRef<FMobileDirectionalLightShaderParameters>& GetNullMobileDirectionalLightShaderParameters()
{
	static TUniformBufferRef<FMobileDirectionalLightShaderParameters> NullLightParams = TUniformBufferRef<FMobileDirectionalLightShaderParameters>::CreateUniformBufferImmediate(FMobileDirectionalLightShaderParameters(), UniformBuffer_MultiFrame);
	return NullLightParams;
}

/**
 * Initialize scene's views.
 * Check visibility, sort translucent items, etc.
 */
void FMobileSceneRenderer::InitViews(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, InitViews);

	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);

	FILCUpdatePrimTaskData ILCTaskData;
	PreVisibilityFrameSetup(RHICmdList);
	ComputeViewVisibility(RHICmdList);
	PostVisibilityFrameSetup(ILCTaskData);

	const bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows;

	if (bDynamicShadows && !IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		// Setup dynamic shadows.
		InitDynamicShadows(RHICmdList);		
	}

	// if we kicked off ILC update via task, wait and finalize.
	if (ILCTaskData.TaskRef.IsValid())
	{
		Scene->IndirectLightingCache.FinalizeCacheUpdates(Scene, *this, ILCTaskData);
	}

	// initialize per-view uniform buffer.  Pass in shadow info as necessary.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		// Initialize the view's RHI resources.
		Views[ViewIndex].InitRHIResources();

		// Create the directional light uniform buffers
		CreateDirectionalLightUniformBuffers(Views[ViewIndex]);
	}

	// Now that the indirect lighting cache is updated, we can update the primitive precomputed lighting buffers.
	UpdatePrimitivePrecomputedLightingBuffers();

	UpdatePostProcessUsageFlags();
	
	OnStartFrame(RHICmdList);
}

/** 
* Renders the view family. 
*/
void FMobileSceneRenderer::Render(FRHICommandListImmediate& RHICmdList)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FMobileSceneRenderer_Render);

	if(!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}

	const ERHIFeatureLevel::Type ViewFeatureLevel = ViewFamily.GetFeatureLevel();

	// Initialize global system textures (pass-through if already initialized).
	GSystemTextures.InitializeTextures(RHICmdList, ViewFeatureLevel);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// Allocate the maximum scene render target space for the current view family.
	SceneContext.Allocate(RHICmdList, ViewFamily);

	//make sure all the targets we're going to use will be safely writable.
	GRenderTargetPool.TransitionTargetsWritable(RHICmdList);

	// Find the visible primitives.
	InitViews(RHICmdList);

	for (int32 ViewExt = 0; ViewExt < ViewFamily.ViewExtensions.Num(); ++ViewExt)
	{
		ViewFamily.ViewExtensions[ViewExt]->PostInitViewFamily_RenderThread(RHICmdList, ViewFamily);
		for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex)
		{
			ViewFamily.ViewExtensions[ViewExt]->PostInitView_RenderThread(RHICmdList, Views[ViewIndex]);
		}
	}

	if (IsRunningRHIInSeparateThread())
	{
		// we will probably stall on occlusion queries, so might as well have the RHI thread and GPU work while we wait.
		// Also when doing RHI thread this is the only spot that will process pending deletes
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	
		extern RHI_API void FlushPipelineStateCache();
		FlushPipelineStateCache();
	}

	// Notify the FX system that the scene is about to be rendered.
	if (Scene->FXSystem && !Views[0].bIsPlanarReflection && ViewFamily.EngineShowFlags.Particles)
	{
		Scene->FXSystem->PreRender(RHICmdList, NULL);
	}

	GRenderTargetPool.VisualizeTexture.OnStartFrame(Views[0]);

	RenderShadowDepthMaps(RHICmdList);

	// Dynamic vertex and index buffers need to be committed before rendering.
	FGlobalDynamicVertexBuffer::Get().Commit();
	FGlobalDynamicIndexBuffer::Get().Commit();

	// This might eventually be a problem with multiple views.
	// Using only view 0 to check to do on-chip transform of alpha.
	FViewInfo& View = Views[0];

	// Default view list
	TArray<const FViewInfo*> ViewList;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++) 
	{
		if (Views[ViewIndex].StereoPass != eSSP_MONOSCOPIC_EYE)
		{
			ViewList.Add(&Views[ViewIndex]);
		}
	}

	const bool bGammaSpace = !IsMobileHDR();
	const bool bRequiresUpscale = !ViewFamily.bUseSeparateRenderTarget && ((uint32)ViewFamily.RenderTarget->GetSizeXY().X > ViewFamily.FamilySizeX || (uint32)ViewFamily.RenderTarget->GetSizeXY().Y > ViewFamily.FamilySizeY);
	// ES2 requires that the back buffer and depth match dimensions.
	// For the most part this is not the case when using scene captures. Thus scene captures always render to scene color target.
	const bool bStereoRenderingAndHMD = View.Family->EngineShowFlags.StereoRendering && View.Family->EngineShowFlags.HMDDistortion;
	const bool bRenderToSceneColor = bStereoRenderingAndHMD || bRequiresUpscale || FSceneRenderer::ShouldCompositeEditorPrimitives(View) || View.bIsSceneCapture || View.bIsReflectionCapture;

	if (!bGammaSpace)
	{
		RenderCustomDepthPass(RHICmdList);
	}

	FTextureRHIParamRef SceneColor = nullptr;

	if (bGammaSpace && !bRenderToSceneColor)
	{
		SceneColor = GetMultiViewSceneColor(SceneContext);
		const FTextureRHIParamRef SceneDepth = (View.bIsMobileMultiViewEnabled) ? SceneContext.MobileMultiViewSceneDepthZ->GetRenderTargetItem().TargetableTexture : static_cast<FTextureRHIRef>(SceneContext.GetSceneDepthTexture());
		SetRenderTarget(RHICmdList, SceneColor, SceneDepth, ESimpleRenderTargetMode::EClearColorAndDepth);
	}
	else
	{
		// Begin rendering to scene color
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EClearColorAndDepth);
		SceneColor = SceneContext.GetSceneColorSurface();
	}

	if (GIsEditor && !View.bIsSceneCapture)
	{
		DrawClearQuad(RHICmdList, Views[0].BackgroundColor);
	}

	RenderMobileBasePass(RHICmdList, ViewList);

	for (int32 ViewExt = 0; ViewExt < ViewFamily.ViewExtensions.Num(); ++ViewExt)
	{
		for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex)
		{
			ViewFamily.ViewExtensions[ViewExt]->PostRenderMobileBasePass_RenderThread(RHICmdList, Views[ViewIndex]);
		}
	}

	// Make a copy of the scene depth if the current hardware doesn't support reading and writing to the same depth buffer
	ConditionalResolveSceneDepth(RHICmdList, View);
	
	if (ViewFamily.EngineShowFlags.Decals && !View.bIsPlanarReflection)
	{
		RenderDecals(RHICmdList);
	}

	// Notify the FX system that opaque primitives have been rendered.
	if (Scene->FXSystem && !Views[0].bIsPlanarReflection && ViewFamily.EngineShowFlags.Particles)
	{
		//#todo-rco: This is switching to another RT!
		Scene->FXSystem->PostRenderOpaque(RHICmdList);
	}

	if (!View.bIsPlanarReflection)
	{
		RenderModulatedShadowProjections(RHICmdList);
	}
	
	// Draw translucency.
	if (ViewFamily.EngineShowFlags.Translucency)
	{
		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);

		// Note: Forward pass has no SeparateTranslucency, so refraction effect order with Translucency is different.
		// Having the distortion applied between two different translucency passes would make it consistent with the deferred pass.
		// This is not done yet.

		if (GetRefractionQuality(ViewFamily) > 0)
		{
			// to apply refraction effect by distorting the scene color
			RenderDistortionES2(RHICmdList);
		}
		RenderTranslucency(RHICmdList, ViewList);
	}

	if (ViewFamily.IsMonoscopicFarFieldEnabled() && ViewFamily.Views.Num() == 3)
	{
		TArray<const FViewInfo*> MonoViewList;
		MonoViewList.Add(&Views[2]);

		RenderMonoscopicFarFieldMask(RHICmdList);
		RenderMobileBasePass(RHICmdList, MonoViewList);
		RenderTranslucency(RHICmdList, MonoViewList);
		CompositeMonoscopicFarField(RHICmdList);
	}

	if (!View.bIsMobileMultiViewDirectEnabled)
	{
		CopyMobileMultiViewSceneColor(RHICmdList);
	}

	static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
	bool bOnChipSunMask =
		GSupportsRenderTargetFormat_PF_FloatRGBA &&
		GSupportsShaderFramebufferFetch &&
		ViewFamily.EngineShowFlags.PostProcessing &&
		((View.bLightShaftUse) || GetMobileDepthOfFieldScale(View) > 0.0 ||
		((ViewFamily.GetShaderPlatform() == SP_METAL) && (CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() > 1 : false))
		);

	if (!bGammaSpace && bOnChipSunMask)
	{
		// Convert alpha from depth to circle of confusion with sunshaft intensity.
		// This is done before resolve on hardware with framebuffer fetch.
		// This will break when PrePostSourceViewportSize is not full size.
		FIntPoint PrePostSourceViewportSize = SceneContext.GetBufferSizeXY();

		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FRenderingCompositePass* PostProcessSunMask = CompositeContext.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMaskES2(PrePostSourceViewportSize, true));
		CompositeContext.Process(PostProcessSunMask, TEXT("OnChipAlphaTransform"));
	}

	bool bKeepDepthContent = false;

	if (!bGammaSpace || bRenderToSceneColor)
	{
		// Resolve the scene color for post processing.
		RHICmdList.CopyToResolveTarget(SceneContext.GetSceneColorSurface(), SceneContext.GetSceneColorTexture(), true, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

		// On PowerVR we see flickering of shadows and depths not updating correctly if targets are discarded.
		// See CVarMobileForceDepthResolve use in ConditionalResolveSceneDepth.
		const bool bForceDepthResolve = CVarMobileForceDepthResolve.GetValueOnRenderThread() == 1;
		const bool bSeparateTranslucencyActive = IsMobileSeparateTranslucencyActive(View);

		bKeepDepthContent = bForceDepthResolve || bPostProcessUsesDepthTexture || bSeparateTranslucencyActive ||
			(View.bIsSceneCapture && (ViewFamily.SceneCaptureSource == ESceneCaptureSource::SCS_SceneColorHDR || ViewFamily.SceneCaptureSource == ESceneCaptureSource::SCS_SceneColorSceneDepth));
	}

	// Drop depth and stencil before post processing to avoid export.
	if (!bKeepDepthContent)
	{
		RHICmdList.DiscardRenderTargets(true, true, 0);
	}
	
	if (ViewFamily.bResolveScene)
	{
		if (!bGammaSpace)
		{
			// Finish rendering for each view, or the full stereo buffer if enabled
			{
				SCOPED_DRAW_EVENT(RHICmdList, PostProcessing);
				SCOPE_CYCLE_COUNTER(STAT_FinishRenderViewTargetTime);
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
					GPostProcessing.ProcessES2(RHICmdList, Views[ViewIndex], bOnChipSunMask);
				}
			}
		}
		else if (bRenderToSceneColor)
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				BasicPostProcess(RHICmdList, Views[ViewIndex], bRequiresUpscale, FSceneRenderer::ShouldCompositeEditorPrimitives(Views[ViewIndex]));
			}
		}
	}

	RenderFinish(RHICmdList);
}

// Perform simple upscale and/or editor primitive composite if the fully-featured post process is not in use.
void FMobileSceneRenderer::BasicPostProcess(FRHICommandListImmediate& RHICmdList, FViewInfo &View, bool bDoUpscale, bool bDoEditorPrimitives)
{
	FRenderingCompositePassContext CompositeContext(RHICmdList, View);
	FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

	const bool bBlitRequired = !bDoUpscale && !bDoEditorPrimitives;

	if (bDoUpscale || bBlitRequired)
	{	// blit from sceneRT to view family target, simple bilinear if upscaling otherwise point filtered.
		uint32 UpscaleQuality = bDoUpscale ? 1 : 0;
		FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessUpscale(View, UpscaleQuality));

		Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
		Node->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.FinalOutput));

		Context.FinalOutput = FRenderingCompositeOutputRef(Node);
	}

#if WITH_EDITOR
	// Composite editor primitives if we had any to draw and compositing is enabled
	if (bDoEditorPrimitives)
	{
		FRenderingCompositePass* EditorCompNode = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCompositeEditorPrimitives(false));
		EditorCompNode->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
		//Node->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneDepth));
		Context.FinalOutput = FRenderingCompositeOutputRef(EditorCompNode);
	}
#endif

	bool bStereoRenderingAndHMD = View.Family->EngineShowFlags.StereoRendering && View.Family->EngineShowFlags.HMDDistortion;
	if (bStereoRenderingAndHMD)
	{
		FRenderingCompositePass* Node = NULL;
		const EHMDDeviceType::Type DeviceType = GEngine->XRSystem->GetHMDDevice() ? GEngine->XRSystem->GetHMDDevice()->GetHMDDeviceType() : EHMDDeviceType::DT_ES2GenericStereoMesh;
		if (DeviceType == EHMDDeviceType::DT_ES2GenericStereoMesh ||
			DeviceType == EHMDDeviceType::DT_OculusRift ||
			DeviceType == EHMDDeviceType::DT_GoogleVR) // PC Preview
		{
			Node = Context.Graph.RegisterPass(new FRCPassPostProcessHMD());
		}

		if (Node)
		{
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}
	}

	// currently created on the heap each frame but View.Family->RenderTarget could keep this object and all would be cleaner
	TRefCountPtr<IPooledRenderTarget> Temp;
	FSceneRenderTargetItem Item;
	Item.TargetableTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
	Item.ShaderResourceTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();

	FPooledRenderTargetDesc Desc;

	Desc.Extent = View.Family->RenderTarget->GetSizeXY();
	// todo: this should come from View.Family->RenderTarget
	Desc.Format = PF_B8G8R8A8;
	Desc.NumMips = 1;

	GRenderTargetPool.CreateUntrackedElement(Desc, Temp, Item);

	Context.FinalOutput.GetOutput()->PooledRenderTarget = Temp;
	Context.FinalOutput.GetOutput()->RenderTargetDesc = Desc;

	CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("ES2BasicPostProcess"));
}

void FMobileSceneRenderer::ConditionalResolveSceneDepth(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	SceneContext.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);

	auto ShaderPlatform = ViewFamily.GetShaderPlatform();

	if (IsMobileHDR() 
		&& IsMobilePlatform(ShaderPlatform) 
		&& !IsPCPlatform(ShaderPlatform) // exclude mobile emulation on PC
		&& !View.bIsPlanarReflection)	// exclude depth resolve from planar reflection captures, can't do it reliably more than once per frame
	{
		bool bSceneDepthInAlpha = (SceneContext.GetSceneColor()->GetDesc().Format == PF_FloatRGBA);
		bool bOnChipDepthFetch = (GSupportsShaderDepthStencilFetch || (bSceneDepthInAlpha && GSupportsShaderFramebufferFetch));
		
		const bool bAlwaysResolveDepth = CVarMobileAlwaysResolveDepth.GetValueOnRenderThread() == 1;

		if (!bOnChipDepthFetch || bAlwaysResolveDepth)
		{
			// Only these features require depth texture
			bool bDecals = ViewFamily.EngineShowFlags.Decals && Scene->Decals.Num();
			bool bModulatedShadows = ViewFamily.EngineShowFlags.DynamicShadows && bModulatedShadowsInUse;

			if (bDecals || bModulatedShadows || bAlwaysResolveDepth || View.bUsesSceneDepth)
			{
				SCOPED_DRAW_EVENT(RHICmdList, ConditionalResolveSceneDepth);

				// WEBGL copies depth from SceneColor alpha to a separate texture
				if (ShaderPlatform == SP_OPENGL_ES2_WEBGL)
				{
					if (bSceneDepthInAlpha)
					{
						CopySceneAlpha(RHICmdList, View);
					}
				}
				else
				{
					// Switch target to force hardware flush current depth to texture
					FTextureRHIRef DummySceneColor = GSystemTextures.BlackDummy->GetRenderTargetItem().TargetableTexture;
					FTextureRHIRef DummyDepthTarget = GSystemTextures.DepthDummy->GetRenderTargetItem().TargetableTexture;
					SetRenderTarget(RHICmdList, DummySceneColor, DummyDepthTarget, ESimpleRenderTargetMode::EUninitializedColorClearDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);

					if(CVarMobileForceDepthResolve.GetValueOnRenderThread() != 0)
					{
						FGraphicsPipelineStateInitializer GraphicsPSOInit;
						RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
						GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
						GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
						GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

						// for devices that do not support framebuffer fetch we rely on undocumented behavior:
						// Depth reading features will have the depth bound as an attachment AND as a sampler this means
						// some driver implementations will ignore our attempts to resolve, here we draw with the depth texture to force a resolve.
						// See UE-37809 for a description of the desired fix.
						// The results of this draw are irrelevant.
						TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
						TShaderMapRef<FScreenPS> PixelShader(View.ShaderMap);
					
						GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
						GraphicsPSOInit.PrimitiveType = PT_TriangleList;

						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

						ScreenVertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
						PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SceneContext.GetSceneDepthTexture());
						DrawRectangle(
							RHICmdList,
							0, 0,
							0, 0,
							0, 0,
							1, 1,
							FIntPoint(1, 1),
							FIntPoint(1, 1),
							*ScreenVertexShader,
							EDRF_UseTriangleOptimization);
					}
				}
			}
		}
	}
}

void FMobileSceneRenderer::CreateDirectionalLightUniformBuffers(FSceneView& SceneView)
{
	bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows;

	// First array entry is used for primitives with no lighting channel set
	SceneView.MobileDirectionalLightUniformBuffers[0] = TUniformBufferRef<FMobileDirectionalLightShaderParameters>::CreateUniformBufferImmediate(FMobileDirectionalLightShaderParameters(), UniformBuffer_SingleFrame);

	// Fill in the other entries based on the lights
	for (int32 ChannelIdx = 0; ChannelIdx < ARRAY_COUNT(Scene->MobileDirectionalLights); ChannelIdx++)
	{
		FMobileDirectionalLightShaderParameters Params;

		FLightSceneInfo* Light = Scene->MobileDirectionalLights[ChannelIdx];
		if (Light)
		{
			Params.DirectionalLightColor = Light->Proxy->GetColor() / PI;
			Params.DirectionalLightDirection = -Light->Proxy->GetDirection();

			if (bDynamicShadows && VisibleLightInfos.IsValidIndex(Light->Id) && VisibleLightInfos[Light->Id].AllProjectedShadows.Num() > 0)
			{
				const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& DirectionalLightShadowInfos = VisibleLightInfos[Light->Id].AllProjectedShadows;

				static_assert(MAX_MOBILE_SHADOWCASCADES <= 4, "more than 4 cascades not supported by the shader and uniform buffer");
				{
					const FProjectedShadowInfo* ShadowInfo = DirectionalLightShadowInfos[0];
					const FIntPoint ShadowBufferResolution = ShadowInfo->GetShadowBufferResolution();
					const FVector4 ShadowBufferSizeValue((float)ShadowBufferResolution.X, (float)ShadowBufferResolution.Y, 1.0f / (float)ShadowBufferResolution.X, 1.0f / (float)ShadowBufferResolution.Y);

					Params.DirectionalLightShadowTexture = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture.GetReference();
					Params.DirectionalLightShadowTransition = 1.0f / ShadowInfo->ComputeTransitionSize();
					Params.DirectionalLightShadowSize = ShadowBufferSizeValue;
				}

				const int32 NumShadowsToCopy = FMath::Min(DirectionalLightShadowInfos.Num(), MAX_MOBILE_SHADOWCASCADES);
				for (int32 i = 0; i < NumShadowsToCopy; ++i)
				{
					const FProjectedShadowInfo* ShadowInfo = DirectionalLightShadowInfos[i];
					Params.DirectionalLightScreenToShadow[i] = ShadowInfo->GetScreenToShadowMatrix(SceneView);
					Params.DirectionalLightShadowDistances[i] = ShadowInfo->CascadeSettings.SplitFar;
				}
			}
		}

		SceneView.MobileDirectionalLightUniformBuffers[ChannelIdx + 1] = TUniformBufferRef<FMobileDirectionalLightShaderParameters>::CreateUniformBufferImmediate(Params, UniformBuffer_SingleFrame);
	}
}
class FCopyMobileMultiViewSceneColorPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyMobileMultiViewSceneColorPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FCopyMobileMultiViewSceneColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		MobileMultiViewSceneColorTexture.Bind(Initializer.ParameterMap, TEXT("MobileMultiViewSceneColorTexture"));
		MobileMultiViewSceneColorTextureSampler.Bind(Initializer.ParameterMap, TEXT("MobileMultiViewSceneColorTextureSampler"));
	}

	FCopyMobileMultiViewSceneColorPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FUniformBufferRHIParamRef ViewUniformBuffer, FTextureRHIRef InMobileMultiViewSceneColorTexture)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, ViewUniformBuffer);
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			MobileMultiViewSceneColorTexture,
			MobileMultiViewSceneColorTextureSampler,
			TStaticSamplerState<SF_Bilinear>::GetRHI(),
			InMobileMultiViewSceneColorTexture);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MobileMultiViewSceneColorTexture;
		Ar << MobileMultiViewSceneColorTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter MobileMultiViewSceneColorTexture;
	FShaderResourceParameter MobileMultiViewSceneColorTextureSampler;
};

IMPLEMENT_SHADER_TYPE(, FCopyMobileMultiViewSceneColorPS, TEXT("/Engine/Private/MobileMultiView.usf"), TEXT("MainPS"), SF_Pixel);

void FMobileSceneRenderer::CopyMobileMultiViewSceneColor(FRHICommandListImmediate& RHICmdList)
{
	if (!Views[0].bIsMobileMultiViewEnabled)
	{
		return;
	}

	RHICmdList.DiscardRenderTargets(true, true, 0);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// Switching from the multi-view scene color render target array to side by side scene color
	SetRenderTarget(RHICmdList, ViewFamily.RenderTarget->GetRenderTargetTexture(), SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EClearColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop, true);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FCopyMobileMultiViewSceneColorPS> PixelShader(ShaderMap);
	extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];

		// Multi-view color target is our input texture array
		PixelShader->SetParameters(RHICmdList, View.ViewUniformBuffer, SceneContext.MobileMultiViewSceneColor->GetRenderTargetItem().ShaderResourceTexture);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Min.X + View.ViewRect.Width(), View.ViewRect.Min.Y + View.ViewRect.Height(), 1.0f);
		const FIntPoint TargetSize(View.ViewRect.Width(), View.ViewRect.Height());

		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			TargetSize,
			TargetSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
}

void FMobileSceneRenderer::UpdatePostProcessUsageFlags()
{
	bPostProcessUsesDepthTexture = false;
	// Find out whether post-process materials require SceneDepth lookups, otherwise renderer can discard depth buffer before starting post-processing pass
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FBlendableManager& BlendableManager = Views[ViewIndex].FinalPostProcessSettings.BlendableManager;
		FBlendableEntry* BlendableIt = nullptr;

		while (FPostProcessMaterialNode* DataPtr = BlendableManager.IterateBlendables<FPostProcessMaterialNode>(BlendableIt))
		{
			if (DataPtr->IsValid())
			{
				FMaterialRenderProxy* Proxy = DataPtr->GetMaterialInterface()->GetRenderProxy(false);
				check(Proxy);

				const FMaterial* Material = Proxy->GetMaterial(Views[ViewIndex].GetFeatureLevel());
				check(Material);

				if (Material->MaterialUsesSceneDepthLookup_RenderThread())
				{
					bPostProcessUsesDepthTexture = true;
					break;
				}
			}
		}
	}
}
