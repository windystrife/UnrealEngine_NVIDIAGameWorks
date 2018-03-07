// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRenderingPolicy.h"
#include "UniformBuffer.h"
#include "Shader.h"
#include "ShowFlags.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "EngineGlobals.h"
#include "RHIStaticStates.h"
#include "RHIUtilities.h"
#include "SceneView.h"
#include "SceneUtils.h"
#include "Engine/Engine.h"
#include "SlateShaders.h"
#include "Rendering/SlateRenderer.h"
#include "SlateRHIRenderer.h"
#include "SlateMaterialShader.h"
#include "SlateUTextureResource.h"
#include "SlateMaterialResource.h"
#include "SlateUpdatableBuffer.h"
#include "SlatePostProcessor.h"
#include "Modules/ModuleManager.h"
#include "PipelineStateCache.h"
#include "Math/RandomStream.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"

extern void UpdateNoiseTextureParameters(FViewUniformShaderParameters& ViewUniformShaderParameters);

DECLARE_CYCLE_STAT(TEXT("Update Buffers RT"), STAT_SlateUpdateBufferRTTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("PreFill Buffers RT"), STAT_SlatePreFullBufferRTTime, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Layers"), STAT_SlateNumLayers, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Batches"), STAT_SlateNumBatches, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Vertices"), STAT_SlateVertexCount, STATGROUP_Slate);

DECLARE_CYCLE_STAT(TEXT("Slate RT: Draw Call"), STAT_SlateRTDrawCall, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate RT: Custom Draw"), STAT_SlateRTCustomDraw, STATGROUP_Slate);

DECLARE_DWORD_COUNTER_STAT(TEXT("Clips (Scissor)"), STAT_SlateScissorClips, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Clips (Stencil)"), STAT_SlateStencilClips, STATGROUP_Slate);

#if UE_BUILD_DEBUG
int32 SlateEnableDrawEvents = 1;
#else
int32 SlateEnableDrawEvents = 0;
#endif
static FAutoConsoleVariableRef CVarSlateEnableDrawEvents(TEXT("Slate.EnableDrawEvents"), SlateEnableDrawEvents, TEXT("."), ECVF_Default);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define SLATE_DRAW_EVENT(RHICmdList, EventName) SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, EventName, SlateEnableDrawEvents);
#else
	#define SLATE_DRAW_EVENT(RHICmdList, EventName)
#endif

TAutoConsoleVariable<int32> CVarSlateAbsoluteIndices(
	TEXT("Slate.AbsoluteIndices"),
	0,
	TEXT("0: Each element first vertex index starts at 0 (default), 1: Use absolute indices, simplifies draw call setup on RHIs that do not support BaseVertex"),
	ECVF_Default
);

FSlateRHIRenderingPolicy::FSlateRHIRenderingPolicy(TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, TOptional<int32> InitialBufferSize)
	: FSlateRenderingPolicy(InSlateFontServices, 0)
	, PostProcessor(new FSlatePostProcessor)
	, ResourceManager(InResourceManager)
	, bGammaCorrect(true)
	, InitialBufferSizeOverride(InitialBufferSize)
{
	InitResources();
}

void FSlateRHIRenderingPolicy::InitResources()
{
	int32 NumVertices = 100;

	if ( InitialBufferSizeOverride.IsSet() )
	{
		NumVertices = InitialBufferSizeOverride.GetValue();
	}
	else if ( GConfig )
	{
		int32 NumVertsInConfig = 0;
		if ( GConfig->GetInt(TEXT("SlateRenderer"), TEXT("NumPreallocatedVertices"), NumVertsInConfig, GEngineIni) )
		{
			NumVertices = NumVertsInConfig;
		}
	}

	// Always create a little space but never allow it to get too high
#if !SLATE_USE_32BIT_INDICES
	NumVertices = FMath::Clamp(NumVertices, 100, 65535);
#else
	NumVertices = FMath::Clamp(NumVertices, 100, 1000000);
#endif

	UE_LOG(LogSlate, Verbose, TEXT("Allocating space for %d vertices"), NumVertices);

	VertexBuffers.Init(NumVertices);
	IndexBuffers.Init(NumVertices);
}

void FSlateRHIRenderingPolicy::ReleaseResources()
{
	VertexBuffers.Destroy();
	IndexBuffers.Destroy();
}

void FSlateRHIRenderingPolicy::BeginDrawingWindows()
{
	check( IsInRenderingThread() );
}

void FSlateRHIRenderingPolicy::EndDrawingWindows()
{
	check( IsInParallelRenderingThread() );
}

struct FSlateUpdateVertexAndIndexBuffers : public FRHICommand<FSlateUpdateVertexAndIndexBuffers>
{
	FVertexBufferRHIRef VertexBufferRHI;
	FIndexBufferRHIRef IndexBufferRHI;
	FSlateBatchData& BatchData;
	bool bAbsoluteIndices;

	FSlateUpdateVertexAndIndexBuffers(TSlateElementVertexBuffer<FSlateVertex>& InVertexBuffer, FSlateElementIndexBuffer& InIndexBuffer, FSlateBatchData& InBatchData, bool bInAbsoluteIndices)
		: VertexBufferRHI(InVertexBuffer.VertexBufferRHI)
		, IndexBufferRHI(InIndexBuffer.IndexBufferRHI)
		, BatchData(InBatchData)
		, bAbsoluteIndices(bInAbsoluteIndices)
	{
		check(IsInRenderingThread());
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		SCOPE_CYCLE_COUNTER(STAT_SlateUpdateBufferRTTime);

		const int32 NumBatchedVertices = BatchData.GetNumBatchedVertices();
		const int32 NumBatchedIndices = BatchData.GetNumBatchedIndices();

		int32 RequiredVertexBufferSize = NumBatchedVertices*sizeof(FSlateVertex);
		uint8* VertexBufferData = (uint8*)GDynamicRHI->RHILockVertexBuffer( VertexBufferRHI, 0, RequiredVertexBufferSize, RLM_WriteOnly );

		uint32 RequiredIndexBufferSize = NumBatchedIndices*sizeof(SlateIndex);		
		uint8* IndexBufferData = (uint8*)GDynamicRHI->RHILockIndexBuffer( IndexBufferRHI, 0, RequiredIndexBufferSize, RLM_WriteOnly );

		BatchData.FillVertexAndIndexBuffer( VertexBufferData, IndexBufferData, bAbsoluteIndices );

		GDynamicRHI->RHIUnlockVertexBuffer( VertexBufferRHI );
		GDynamicRHI->RHIUnlockIndexBuffer( IndexBufferRHI );
	}
};

void FSlateRHIRenderingPolicy::UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData)
{
	UpdateVertexAndIndexBuffers(RHICmdList, InBatchData, VertexBuffers, IndexBuffers);
}

void FSlateRHIRenderingPolicy::UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData, const TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe>& RenderHandle)
{
	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	FCachedRenderBuffers* Buffers = ResourceManager->FindOrCreateCachedBuffersForHandle(RenderHandle);

	UpdateVertexAndIndexBuffers(RHICmdList, InBatchData, Buffers->VertexBuffer, Buffers->IndexBuffer);
}

void FSlateRHIRenderingPolicy::ReleaseCachingResourcesFor(FRHICommandListImmediate& RHICmdList, const ILayoutCache* Cacher)
{
	ResourceManager->ReleaseCachingResourcesFor(RHICmdList, Cacher);
}

void FSlateRHIRenderingPolicy::UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData, TSlateElementVertexBuffer<FSlateVertex>& VertexBuffer, FSlateElementIndexBuffer& IndexBuffer)
{
	SCOPE_CYCLE_COUNTER( STAT_SlateUpdateBufferRTTime );

	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	const int32 NumVertices = InBatchData.GetNumBatchedVertices();
	const int32 NumIndices = InBatchData.GetNumBatchedIndices();

	if( InBatchData.GetRenderBatches().Num() > 0  && NumVertices > 0 && NumIndices > 0)
	{
		bool bShouldShrinkResources = false;
		bool bAbsoluteIndices = CVarSlateAbsoluteIndices.GetValueOnRenderThread() != 0;

		VertexBuffer.PreFillBuffer(NumVertices, bShouldShrinkResources);
		IndexBuffer.PreFillBuffer(NumIndices, bShouldShrinkResources);

		if(!IsRunningRHIInSeparateThread() || RHICmdList.Bypass())
		{
			uint8* VertexBufferData = (uint8*)VertexBuffer.LockBuffer_RenderThread(NumVertices);
			uint8* IndexBufferData =  (uint8*)IndexBuffer.LockBuffer_RenderThread(NumIndices);
									
			InBatchData.FillVertexAndIndexBuffer( VertexBufferData, IndexBufferData, bAbsoluteIndices );
	
			VertexBuffer.UnlockBuffer_RenderThread();
			IndexBuffer.UnlockBuffer_RenderThread();
		}
		else
		{
			new (RHICmdList.AllocCommand<FSlateUpdateVertexAndIndexBuffers>()) FSlateUpdateVertexAndIndexBuffers(VertexBuffer, IndexBuffer, InBatchData, bAbsoluteIndices);
		}
	}

	checkSlow(VertexBuffer.GetBufferUsageSize() <= VertexBuffer.GetBufferSize());
	checkSlow(IndexBuffer.GetBufferUsageSize() <= IndexBuffer.GetBufferSize());

	SET_DWORD_STAT( STAT_SlateNumLayers, InBatchData.GetNumLayers() );
	SET_DWORD_STAT( STAT_SlateNumBatches, InBatchData.GetRenderBatches().Num() );
	SET_DWORD_STAT( STAT_SlateVertexCount, InBatchData.GetNumBatchedVertices() );
}

static FSceneView* CreateSceneView( FSceneViewFamilyContext* ViewFamilyContext, FSlateBackBuffer& BackBuffer, const FMatrix& ViewProjectionMatrix )
{
	// In loading screens, the engine is NULL, so we skip out.
	if (GEngine == nullptr)
	{
		return nullptr;
	}

	FIntRect ViewRect(FIntPoint(0, 0), BackBuffer.GetSizeXY());

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamilyContext;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = ViewProjectionMatrix;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView* View = new FSceneView( ViewInitOptions );
	ViewFamilyContext->Views.Add( View );

	const FIntPoint BufferSize = BackBuffer.GetSizeXY();

	// Create the view's uniform buffer.
	FViewUniformShaderParameters ViewUniformShaderParameters;

	View->SetupCommonViewUniformBufferParameters(
		ViewUniformShaderParameters,
		BufferSize,
		1,
		ViewRect,
		View->ViewMatrices,
		FViewMatrices()
	);

	ViewUniformShaderParameters.WorldViewOrigin = View->ViewMatrices.GetViewOrigin();
	

	ERHIFeatureLevel::Type RHIFeatureLevel = View->GetFeatureLevel();

	ViewUniformShaderParameters.MobilePreviewMode =
		(GIsEditor &&
		(RHIFeatureLevel == ERHIFeatureLevel::ES2 || RHIFeatureLevel == ERHIFeatureLevel::ES3_1) &&
		GMaxRHIFeatureLevel > ERHIFeatureLevel::ES3_1) ? 1.0f : 0.0f;

	UpdateNoiseTextureParameters(ViewUniformShaderParameters);

	View->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(ViewUniformShaderParameters, UniformBuffer_SingleFrame);

	return View;
}

static const FName RendererModuleName("Renderer");

void FSlateRHIRenderingPolicy::DrawElements(
	FRHICommandListImmediate& RHICmdList,
	FSlateBackBuffer& BackBuffer,
	FTexture2DRHIRef& ColorTarget,
	FTexture2DRHIRef& DepthStencilTarget,
	const TArray<FSlateRenderBatch>& RenderBatches, 
	const TArray<FSlateClippingState> RenderClipStates,
	const FSlateRenderingOptions& Options)
{
	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	TArray<FTextureLODGroup> TextureLODGroups;
	if (UDeviceProfileManager::DeviceProfileManagerSingleton)
	{
		if (UDeviceProfile* Profile = UDeviceProfileManager::Get().GetActiveProfile())
		{
			TextureLODGroups = Profile->GetTextureLODSettings()->TextureLODGroups;
		}
	}

	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	float TimeSeconds = FApp::GetCurrentTime() - GStartTime;
	float DeltaTimeSeconds = FApp::GetDeltaTime();
	float RealTimeSeconds =  FPlatformTime::Seconds() - GStartTime;

	static const FEngineShowFlags DefaultShowFlags(ESFIM_Game);

	const float EngineGamma = GEngine ? GEngine->GetDisplayGamma() : 2.2f;
	const float DisplayGamma = bGammaCorrect ? EngineGamma : 1.0f;

#if STATS
	int32 ScissorClips = 0;
	int32 StencilClips = 0;
#endif

	// In order to support MaterialParameterCollections, we need to create multiple FSceneViews for 
	// each possible Scene that we encounter. The following code creates these as separate arrays, where the first 
	// N entries map directly to entries from ActiveScenes. The final entry is added to represent the absence of a
	// valid scene, i.e. a -1 in the SceneIndex parameter of the batch.
	int32 NumScenes = ResourceManager->GetSceneCount() + 1;
	TArray<FSceneView*, TInlineAllocator<3> > SceneViews;
	SceneViews.SetNum(NumScenes);
	TArray<FSceneViewFamilyContext*, TInlineAllocator<3> > SceneViewFamilyContexts;
	SceneViewFamilyContexts.SetNum(NumScenes);
	for (int32 i = 0; i < ResourceManager->GetSceneCount(); i++)
	{
		SceneViewFamilyContexts[i] = new  FSceneViewFamilyContext
			(
				FSceneViewFamily::ConstructionValues
				(
					&BackBuffer,
					ResourceManager->GetSceneAt(i),
					DefaultShowFlags
				)
				.SetWorldTimes(TimeSeconds, DeltaTimeSeconds, RealTimeSeconds)
				.SetGammaCorrection(DisplayGamma)
			);
		SceneViews[i] = CreateSceneView(SceneViewFamilyContexts[i], BackBuffer, Options.ViewProjectionMatrix);
	}

	SceneViewFamilyContexts[NumScenes-1] = new FSceneViewFamilyContext
		(
			FSceneViewFamily::ConstructionValues
			(
				&BackBuffer,
				nullptr,
				DefaultShowFlags
				)
			.SetWorldTimes(TimeSeconds, DeltaTimeSeconds, RealTimeSeconds)
			.SetGammaCorrection(DisplayGamma)
			);
	SceneViews[NumScenes-1] = CreateSceneView(SceneViewFamilyContexts[NumScenes-1], BackBuffer, Options.ViewProjectionMatrix);

	TShaderMapRef<FSlateElementVS> GlobalVertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

	TSlateElementVertexBuffer<FSlateVertex>* VertexBuffer = &VertexBuffers;
	FSlateElementIndexBuffer* IndexBuffer = &IndexBuffers;

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	const FSlateRenderDataHandle* LastHandle = nullptr;

	const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);

#if WITH_SLATE_VISUALIZERS
	FRandomStream BatchColors(1337);
#endif

	const bool bAbsoluteIndices = CVarSlateAbsoluteIndices.GetValueOnRenderThread() != 0;
	const bool bSwitchVerticalAxis = Options.bAllowSwitchVerticalAxis && RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]);

	// This variable tracks the last clipping state, so that if multiple batches have the same clipping state, we don't have to do any work.
	int32 LastClippingIndex = -1;
	// This is the stenciling ref variable we set any time we draw, so that any stencil comparisons use the right mask id.
	uint32 StencilRef = 0;
	// This is an accumulating maskID that we use to track the between batch usage of the stencil buffer, when at 0, or over 255
	// this signals that we need to reset the masking ID, and clear the stencil buffer, as we've used up the available scratch range.
	uint32 MaskingID = 0;

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	// Disable depth/stencil testing by default
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	FVector2D ViewTranslation2D = Options.ViewOffset;

	// Draw each element
	for( int32 BatchIndex = 0; BatchIndex < RenderBatches.Num(); ++BatchIndex )
	{
#if WITH_SLATE_VISUALIZERS
		FLinearColor BatchColor = FLinearColor(BatchColors.GetUnitVector());
#endif

		const FSlateRenderBatch& RenderBatch = RenderBatches[BatchIndex];
		const FSlateRenderDataHandle* RenderHandle = RenderBatch.CachedRenderHandle.Get();

		if ( RenderHandle != LastHandle )
		{
			SLATE_DRAW_EVENT(RHICmdList, ChangingRenderBuffers);

			LastHandle = RenderHandle;

			if ( RenderHandle )
			{
				FCachedRenderBuffers* Buffers = ResourceManager->FindCachedBuffersForHandle(RenderHandle);
				if ( Buffers != nullptr )
				{
					VertexBuffer = &Buffers->VertexBuffer;
					IndexBuffer = &Buffers->IndexBuffer;
				}
			}
			else
			{
				VertexBuffer = &VertexBuffers;
				IndexBuffer = &IndexBuffers;
			}

			checkSlow(VertexBuffer);
			checkSlow(IndexBuffer);
		}

		const FSlateShaderResource* ShaderResource = RenderBatch.Texture;
		const ESlateBatchDrawFlag DrawFlags = RenderBatch.DrawFlags;
		const ESlateDrawEffect DrawEffects = RenderBatch.DrawEffects;
		const ESlateShader::Type ShaderType = RenderBatch.ShaderType;
		const FShaderParams& ShaderParams = RenderBatch.ShaderParams;

		auto UpdateScissorRect = [&RHICmdList,
#if STATS
				&ScissorClips, &StencilClips,
#endif
				&StencilRef, &MaskingID, &BackBuffer, &RenderBatch, &ColorTarget, &DepthStencilTarget, &RenderClipStates, &LastClippingIndex, &ViewTranslation2D, &bSwitchVerticalAxis](FGraphicsPipelineStateInitializer& InGraphicsPSOInit, const FMatrix& ViewProjection, bool bForceStateChange)
		{
			if ( RenderBatch.ClippingIndex != LastClippingIndex || bForceStateChange )
			{
				if ( RenderBatch.ClippingIndex != -1 )
				{
					const FSlateClippingState& ClipState = RenderClipStates[RenderBatch.ClippingIndex];
					if ( ClipState.ScissorRect.IsSet() )
					{
#if STATS
						ScissorClips++;
#endif

						const FSlateClippingZone& ScissorRect = ClipState.ScissorRect.GetValue();

						const FVector2D TopLeft = ScissorRect.TopLeft + ViewTranslation2D;
						const FVector2D BottomRight = ScissorRect.BottomRight + ViewTranslation2D;

						if (bSwitchVerticalAxis)
						{
							const FIntPoint ViewSize = BackBuffer.GetSizeXY();
							const int32 MinY = (ViewSize.Y - BottomRight.Y);
							const int32 MaxY = (ViewSize.Y - TopLeft.Y);
							RHICmdList.SetScissorRect(true, TopLeft.X, MinY, BottomRight.X, MaxY);
						}
						else
						{
							RHICmdList.SetScissorRect(true, TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);
						}
						
						// Disable depth/stencil testing by default
						InGraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
						StencilRef = 0;
					}
					else
					{
#if STATS
						StencilClips++;
#endif

						SLATE_DRAW_EVENT(RHICmdList, StencilClipping);

						check(ClipState.StencilQuads.Num() > 0);

						const TArray<FSlateClippingZone>& StencilQuads = ClipState.StencilQuads;

						// We're going to overflow the masking ID this time, we need to reset the MaskingID to 0.
						// this will cause us to clear the stencil buffer so that we can begin fresh.
						if ((MaskingID + StencilQuads.Num()) > 255)
						{
							MaskingID = 0;
						}

						// We only clear the stencil the first time, and if some how the user draws more than 255 masking quads
						// in a single frame.
						bool bClearStencil = false;
						if (MaskingID == 0)
						{
							bClearStencil = true;

							// We don't want there to be any scissor rect when we clear the stencil
							RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
						}
						else
						{
							// There might be some large - useless stencils, especially in the first couple of stencils if large
							// widgets that clip also contain render targets, so, by setting the scissor to the AABB of the final
							// stencil, we can cut out a lot of work that can't possibly be useful.
							//
							// NOTE - We also round it, because if we don't it can over-eagerly slice off pixels it shouldn't.
							const FSlateClippingZone& MaskQuad = StencilQuads.Last();
							const FSlateRect LastStencilBoundingBox = MaskQuad.GetBoundingBox().Round();

							const FVector2D TopLeft = LastStencilBoundingBox.GetTopLeft() + ViewTranslation2D;
							const FVector2D BottomRight = LastStencilBoundingBox.GetBottomRight() + ViewTranslation2D;

							if (bSwitchVerticalAxis)
							{
								const FIntPoint ViewSize = BackBuffer.GetSizeXY();
								const int32 MinY = (ViewSize.Y - BottomRight.Y);
								const int32 MaxY = (ViewSize.Y - TopLeft.Y);
								RHICmdList.SetScissorRect(true, TopLeft.X, MinY, BottomRight.X, MaxY);
							}
							else
							{
								RHICmdList.SetScissorRect(true, TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);
							}
						}

						// Don't bother setting the render targets unless we actually need to clear them.
						if (bClearStencil || bForceStateChange)
						{
							// Clear current stencil buffer, we use ELoad/EStore, because we need to keep the stencil around.
							FRHIRenderTargetView ColorView(ColorTarget, ERenderTargetLoadAction::ELoad);
							FRHIDepthRenderTargetView DepthStencilView(DepthStencilTarget, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction, bClearStencil ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
							FRHISetRenderTargetsInfo CurrentRTInfo(1, &ColorView, DepthStencilView);
							RHICmdList.SetRenderTargetsAndClear(CurrentRTInfo);
						}
						
						// Start by setting up the stenciling states so that we can write representations of the clipping zones into the stencil buffer only.
						{
							FGraphicsPipelineStateInitializer WriteMaskPSOInit;
							RHICmdList.ApplyCachedRenderTargets(WriteMaskPSOInit);
							WriteMaskPSOInit.BlendState = TStaticBlendStateWriteMask<CW_NONE>::GetRHI();
							WriteMaskPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
							WriteMaskPSOInit.DepthStencilState =
								TStaticDepthStencilState<
								/*bEnableDepthWrite*/ false
								, /*DepthTest*/ CF_Always
								, /*bEnableFrontFaceStencil*/ true
								, /*FrontFaceStencilTest*/ CF_Always
								, /*FrontFaceStencilFailStencilOp*/ SO_Keep
								, /*FrontFaceDepthFailStencilOp*/ SO_Keep
								, /*FrontFacePassStencilOp*/ SO_Replace
								, /*bEnableBackFaceStencil*/ true
								, /*BackFaceStencilTest*/ CF_Always
								, /*BackFaceStencilFailStencilOp*/ SO_Keep
								, /*BackFaceDepthFailStencilOp*/ SO_Keep
								, /*BackFacePassStencilOp*/ SO_Replace
								, /*StencilReadMask*/ 0xFF
								, /*StencilWriteMask*/ 0xFF>::GetRHI();

							TShaderMap<FGlobalShaderType>* MaxFeatureLevelShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

							// Set the new shaders
							TShaderMapRef<FSlateMaskingVS> VertexShader(MaxFeatureLevelShaderMap);
							TShaderMapRef<FSlateMaskingPS> PixelShader(MaxFeatureLevelShaderMap);

							WriteMaskPSOInit.BoundShaderState.VertexDeclarationRHI = GSlateMaskingVertexDeclaration.VertexDeclarationRHI;
							WriteMaskPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
							WriteMaskPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
							WriteMaskPSOInit.PrimitiveType = PT_TriangleStrip;

							FLocalGraphicsPipelineState MaskingGraphicsReplacePSO = RHICmdList.BuildLocalGraphicsPipelineState(WriteMaskPSOInit);
							RHICmdList.SetLocalGraphicsPipelineState(MaskingGraphicsReplacePSO);

							VertexShader->SetViewProjection(RHICmdList, ViewProjection);
							VertexShader->SetVerticalAxisMultiplier(RHICmdList, bSwitchVerticalAxis ? -1.0f : 1.0f);

							// Draw the first stencil using SO_Replace, so that we stomp any pixel with a MaskingID + 1.
							{
								const FSlateClippingZone& MaskQuad = StencilQuads[0];

								RHICmdList.SetStencilRef(MaskingID + 1);

								//TODO Slate If we ever decided to add masking with a texture, we could do that here.
								FVector2D Vertices[4];
								Vertices[0].Set(MaskQuad.TopLeft.X, MaskQuad.TopLeft.Y);
								Vertices[1].Set(MaskQuad.TopRight.X, MaskQuad.TopRight.Y);
								Vertices[2].Set(MaskQuad.BottomLeft.X, MaskQuad.BottomLeft.Y);
								Vertices[3].Set(MaskQuad.BottomRight.X, MaskQuad.BottomRight.Y);
								DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
							}

							// Now setup the pipeline to use SO_SaturatedIncrement, since we've established the initial
							// stencil with SO_Replace, we can safely use SO_SaturatedIncrement, to build up the stencil
							// to the required mask of MaskingID + StencilQuads.Num(), thereby ensuring only the union of
							// all stencils will render pixels.
							{
								WriteMaskPSOInit.DepthStencilState =
									TStaticDepthStencilState<
									/*bEnableDepthWrite*/ false
									, /*DepthTest*/ CF_Always
									, /*bEnableFrontFaceStencil*/ true
									, /*FrontFaceStencilTest*/ CF_Always
									, /*FrontFaceStencilFailStencilOp*/ SO_Keep
									, /*FrontFaceDepthFailStencilOp*/ SO_Keep
									, /*FrontFacePassStencilOp*/ SO_SaturatedIncrement
									, /*bEnableBackFaceStencil*/ true
									, /*BackFaceStencilTest*/ CF_Always
									, /*BackFaceStencilFailStencilOp*/ SO_Keep
									, /*BackFaceDepthFailStencilOp*/ SO_Keep
									, /*BackFacePassStencilOp*/ SO_SaturatedIncrement
									, /*StencilReadMask*/ 0xFF
									, /*StencilWriteMask*/ 0xFF>::GetRHI();

								FLocalGraphicsPipelineState MaskingGraphicsIncrementPSO = RHICmdList.BuildLocalGraphicsPipelineState(WriteMaskPSOInit);
								RHICmdList.SetLocalGraphicsPipelineState(MaskingGraphicsIncrementPSO);
							}
						}

						MaskingID += StencilQuads.Num();

						// Next write the number of quads representing the number of clipping zones have on top of each other.
						for (int32 MaskIndex = 1; MaskIndex < StencilQuads.Num(); MaskIndex++)
						{
							const FSlateClippingZone& MaskQuad = StencilQuads[MaskIndex];

							//TODO Slate If we ever decided to add masking with a texture, we could do that here.
							FVector2D Vertices[4];
							Vertices[0].Set(MaskQuad.TopLeft.X, MaskQuad.TopLeft.Y);
							Vertices[1].Set(MaskQuad.TopRight.X, MaskQuad.TopRight.Y);
							Vertices[2].Set(MaskQuad.BottomLeft.X, MaskQuad.BottomLeft.Y);
							Vertices[3].Set(MaskQuad.BottomRight.X, MaskQuad.BottomRight.Y);
							DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
						}

						// Setup the stenciling state to be read only now, disable depth writes, and restore the color buffer
						// because we're about to go back to rendering widgets "normally", but with the added effect that now
						// we have the stencil buffer bound with a bunch of clipping zones rendered into it.
						{
							FDepthStencilStateRHIParamRef DSMaskRead =
								TStaticDepthStencilState<
								/*bEnableDepthWrite*/ false
								, /*DepthTest*/ CF_Always
								, /*bEnableFrontFaceStencil*/ true
								, /*FrontFaceStencilTest*/ CF_Equal
								, /*FrontFaceStencilFailStencilOp*/ SO_Keep
								, /*FrontFaceDepthFailStencilOp*/ SO_Keep
								, /*FrontFacePassStencilOp*/ SO_Keep
								, /*bEnableBackFaceStencil*/ true
								, /*BackFaceStencilTest*/ CF_Equal
								, /*BackFaceStencilFailStencilOp*/ SO_Keep
								, /*BackFaceDepthFailStencilOp*/ SO_Keep
								, /*BackFacePassStencilOp*/ SO_Keep
								, /*StencilReadMask*/ 0xFF
								, /*StencilWriteMask*/ 0xFF>::GetRHI();

							InGraphicsPSOInit.DepthStencilState = DSMaskRead;

							// We set a StencilRef equal to the number of stenciling/clipping masks,
							// so unless the pixel we're rendering two is on top of a stencil pixel with the same number
							// it's going to get rejected, thereby clipping everything except for the cross-section of
							// all the stenciling quads.
							StencilRef = MaskingID;
						}
					}

					RHICmdList.ApplyCachedRenderTargets(InGraphicsPSOInit);
				}
				else
				{
					RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

					// Disable depth/stencil testing
					InGraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					StencilRef = 0;
				}

				LastClippingIndex = RenderBatch.ClippingIndex;
			}
		};

		if ( EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::Wireframe) )
		{
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Wireframe, CM_None, false>::GetRHI();
		}
		else
		{
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, false>::GetRHI();
		}

		if( !RenderBatch.CustomDrawer.IsValid() )
		{
			FMatrix DynamicOffset = FTranslationMatrix::Make(FVector(RenderBatch.DynamicOffset.X, RenderBatch.DynamicOffset.Y, 0));
			const FMatrix ViewProjection = DynamicOffset * Options.ViewProjectionMatrix;

			UpdateScissorRect(GraphicsPSOInit, ViewProjection, false);

			const uint32 PrimitiveCount = RenderBatch.DrawPrimitiveType == ESlateDrawPrimitive::LineList ? RenderBatch.NumIndices / 2 : RenderBatch.NumIndices / 3; 

			ESlateShaderResource::Type ResourceType = ShaderResource ? ShaderResource->GetType() : ESlateShaderResource::Invalid;
			if( ResourceType != ESlateShaderResource::Material && ShaderType != ESlateShader::PostProcess ) 
			{
				SLATE_DRAW_EVENT(RHICmdList, TextureBatch);

				check(RenderBatch.NumIndices > 0);
				FSlateElementPS* PixelShader = nullptr;

				const bool bUseInstancing = RenderBatch.InstanceCount > 1 && RenderBatch.InstanceData != nullptr;
				check(bUseInstancing == false);

#if WITH_SLATE_VISUALIZERS
				FSlateDebugBatchingPS* BatchingPixelShader = nullptr;
				if ( CVarShowSlateBatching.GetValueOnRenderThread() != 0 )
				{
					BatchingPixelShader = *TShaderMapRef<FSlateDebugBatchingPS>(ShaderMap);
					PixelShader = BatchingPixelShader;
				}
				else
#endif
				{
					PixelShader = GetTexturePixelShader(ShaderMap, ShaderType, DrawEffects);
				}

#if WITH_SLATE_VISUALIZERS
				if ( CVarShowSlateBatching.GetValueOnRenderThread() != 0 )
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
				}
				else if ( CVarShowSlateOverdraw.GetValueOnRenderThread() != 0 )
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
				}
				else
#endif
				{
					GraphicsPSOInit.BlendState =
						EnumHasAllFlags( DrawFlags, ESlateBatchDrawFlag::NoBlending )
						? TStaticBlendState<>::GetRHI()
						: ( EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::PreMultipliedAlpha )
							? TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()
							: TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI() )
						;
				}

				if ( EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::Wireframe) || Options.bWireFrame )
				{
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Wireframe, CM_None, false>::GetRHI();

					if ( Options.bWireFrame )
					{
						GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					}
				}
				else
				{
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, false>::GetRHI();
				}

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSlateVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*GlobalVertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
				GraphicsPSOInit.PrimitiveType = GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				RHICmdList.SetStencilRef(StencilRef);

				GlobalVertexShader->SetViewProjection(RHICmdList, ViewProjection);
				GlobalVertexShader->SetVerticalAxisMultiplier(RHICmdList, bSwitchVerticalAxis ? -1.0f : 1.0f);

#if WITH_SLATE_VISUALIZERS
				if (CVarShowSlateBatching.GetValueOnRenderThread() != 0)
				{
					BatchingPixelShader->SetBatchColor(RHICmdList, BatchColor);
				}
#endif

				FSamplerStateRHIParamRef SamplerState = BilinearClamp;
				FTextureRHIParamRef TextureRHI = GWhiteTexture->TextureRHI;
				if (ShaderResource)
				{
					ETextureSamplerFilter Filter = ETextureSamplerFilter::Bilinear;

					if (ResourceType == ESlateShaderResource::TextureObject)
					{
						FSlateBaseUTextureResource* TextureObjectResource = (FSlateBaseUTextureResource*)ShaderResource;

						TextureRHI = TextureObjectResource->AccessRHIResource();

						if (UTexture* TextureObj = TextureObjectResource->TextureObject)
						{
							Filter = GetSamplerFilter(TextureLODGroups, TextureObj);
						}
					}
					else
					{
						FTextureRHIParamRef NativeTextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)ShaderResource)->GetTypedResource();
						// Atlas textures that have no content are never initialized but null textures are invalid on many platforms.
						TextureRHI = NativeTextureRHI ? NativeTextureRHI : (FTextureRHIParamRef)GWhiteTexture->TextureRHI;
					}

					if (EnumHasAllFlags(DrawFlags, (ESlateBatchDrawFlag::TileU | ESlateBatchDrawFlag::TileV)))
					{
						switch (Filter)
						{
						case ETextureSamplerFilter::Point:
							SamplerState = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicPoint:
							SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicLinear:
							SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						default:
							SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						}
					}
					else if (EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::TileU))
					{
						switch (Filter)
						{
						case ETextureSamplerFilter::Point:
							SamplerState = TStaticSamplerState<SF_Point, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicPoint:
							SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicLinear:
							SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
							break;
						default:
							SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
							break;
						}
					}
					else if (EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::TileV))
					{
						switch (Filter)
						{
						case ETextureSamplerFilter::Point:
							SamplerState = TStaticSamplerState<SF_Point, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicPoint:
							SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicLinear:
							SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						default:
							SamplerState = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
							break;
						}
					}
					else
					{
						switch (Filter)
						{
						case ETextureSamplerFilter::Point:
							SamplerState = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicPoint:
							SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							break;
						case ETextureSamplerFilter::Trilinear:
							SamplerState = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							break;
						case ETextureSamplerFilter::AnisotropicLinear:
							SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							break;
						default:
							SamplerState = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							break;
						}
					}
				}
                
				PixelShader->SetTexture(RHICmdList, TextureRHI, SamplerState);
				PixelShader->SetShaderParams(RHICmdList, ShaderParams.PixelParams);
				PixelShader->SetDisplayGamma(RHICmdList, EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::NoGamma ) ? 1.0f : DisplayGamma);
				PixelShader->SetInvertAlpha(RHICmdList, EnumHasAllFlags(DrawEffects, ESlateDrawEffect::InvertAlpha ) ? true : false );

				SCOPE_CYCLE_COUNTER(STAT_SlateRTDrawCall);

				// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
				if (!GRHISupportsBaseVertexIndex && !bAbsoluteIndices)
				{
					RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, RenderBatch.VertexOffset * sizeof(FSlateVertex));
					RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, RenderBatch.InstanceCount);
				}
				else
				{
					uint32 VertexOffset = bAbsoluteIndices ? 0 : RenderBatch.VertexOffset; 
					RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, 0);
					RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, RenderBatch.InstanceCount);
				}
			}
			else if (GEngine && ShaderResource && ShaderResource->GetType() == ESlateShaderResource::Material && ShaderType != ESlateShader::PostProcess)
			{
				SLATE_DRAW_EVENT(RHICmdList, MaterialBatch);

				check(RenderBatch.NumIndices > 0);
				// Note: This code is only executed if the engine is loaded (in early loading screens attempting to use a material is unsupported
				int ActiveSceneIndex = RenderBatch.SceneIndex;

				// We are assuming at this point that the SceneIndex from the batch is either -1, meaning no scene or a valid scene.
				// We set up the "no scene" option as the last SceneView in the array above.
				if (RenderBatch.SceneIndex == -1)
				{
					ActiveSceneIndex = NumScenes - 1;
				}
				else if (RenderBatch.SceneIndex >= ResourceManager->GetSceneCount())
				{
					// Ideally we should never hit this scenario, but given that Paragon may be using cached
					// render batches and is running into this daily, for this branch we should
					// just ignore the scene if the index is invalid. Note that the
					// MaterialParameterCollections will not be correct for this scene, should they be
					// used.
					ActiveSceneIndex = NumScenes - 1;
#if UE_BUILD_DEBUG
	#if WITH_EDITOR
					UE_LOG(LogSlate, Error, TEXT("Invalid scene index in batch: %d of %d known scenes!"), RenderBatch.SceneIndex, ResourceManager->GetSceneCount());
	#endif
#endif
				}

				// Handle the case where we skipped out early above
				if (SceneViews[ActiveSceneIndex] == nullptr)
				{
					continue;
				}

				const FSceneView& ActiveSceneView = *SceneViews[ActiveSceneIndex];

				FSlateMaterialResource* MaterialShaderResource = (FSlateMaterialResource*)ShaderResource;
				if (MaterialShaderResource->GetMaterialObject() != nullptr)
				{
#if !UE_BUILD_SHIPPING
					// pending kill objects may still be rendered for a frame so it is valid for the check to pass
					const bool bEvenIfPendingKill = true;
					// This test needs to be thread safe.  It doesnt give us as many chances to trap bugs here but it is still useful
					const bool bThreadSafe = true;
					checkf(MaterialShaderResource->MaterialObjectWeakPtr.IsValid(bEvenIfPendingKill, bThreadSafe), TEXT("Material %s has become invalid.  This means the resource was garbage collected while slate was using it"), *MaterialShaderResource->MaterialName.ToString());
#endif
					FMaterialRenderProxy* MaterialRenderProxy = MaterialShaderResource->GetRenderProxy();

					const FMaterial* Material = MaterialRenderProxy->GetMaterial(ActiveSceneView.GetFeatureLevel());

					FSlateMaterialShaderPS* PixelShader = GetMaterialPixelShader(Material, ShaderType, DrawEffects);

					const bool bUseInstancing = RenderBatch.InstanceCount > 0 && RenderBatch.InstanceData != nullptr;
					FSlateMaterialShaderVS* VertexShader = GetMaterialVertexShader(Material, bUseInstancing);

					if (VertexShader && PixelShader)
					{
#if WITH_SLATE_VISUALIZERS
						if (CVarShowSlateBatching.GetValueOnRenderThread() != 0)
						{
							FSlateDebugBatchingPS* BatchingPixelShader = *TShaderMapRef<FSlateDebugBatchingPS>(ShaderMap);

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = bUseInstancing ? GSlateInstancedVertexDeclaration.VertexDeclarationRHI : GSlateVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*GlobalVertexShader);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(BatchingPixelShader);

							BatchingPixelShader->SetBatchColor(RHICmdList, BatchColor);
							GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
						}
						else if (CVarShowSlateOverdraw.GetValueOnRenderThread() != 0)
						{
							FSlateElementPS* OverdrawPixelShader = *TShaderMapRef<FSlateDebugOverdrawPS>(ShaderMap);

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = bUseInstancing ? GSlateInstancedVertexDeclaration.VertexDeclarationRHI : GSlateVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*GlobalVertexShader);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(OverdrawPixelShader);

							GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
						}
						else
#endif
						{
							PixelShader->SetBlendState(GraphicsPSOInit, Material);
							FSlateShaderResource* MaskResource = MaterialShaderResource->GetTextureMaskResource();
							if (MaskResource)
							{
								GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI();
							}

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = bUseInstancing ? GSlateInstancedVertexDeclaration.VertexDeclarationRHI : GSlateVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
							GraphicsPSOInit.PrimitiveType = GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType);

							FLocalGraphicsPipelineState BaseGraphicsPSO = RHICmdList.BuildLocalGraphicsPipelineState(GraphicsPSOInit);
							RHICmdList.SetLocalGraphicsPipelineState(BaseGraphicsPSO);

							RHICmdList.SetStencilRef(StencilRef);

							VertexShader->SetViewProjection(RHICmdList, ViewProjection);
							VertexShader->SetVerticalAxisMultiplier(RHICmdList, bSwitchVerticalAxis ? -1.0f : 1.0f);
							VertexShader->SetMaterialShaderParameters(RHICmdList, ActiveSceneView, MaterialRenderProxy, Material);

							PixelShader->SetParameters(RHICmdList, ActiveSceneView, MaterialRenderProxy, Material, ShaderParams.PixelParams);
							PixelShader->SetDisplayGamma(RHICmdList, EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::NoGamma) ? 1.0f : DisplayGamma);

							if (MaskResource)
							{
								FTexture2DRHIRef TextureRHI;
								TextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)MaskResource)->GetTypedResource();

								PixelShader->SetAdditionalTexture(RHICmdList, TextureRHI, BilinearClamp);
							}
						}

						if (bUseInstancing)
						{
							uint32 InstanceCount = RenderBatch.InstanceCount;

							if (GRHISupportsInstancing)
							{
								FSlateUpdatableInstanceBuffer* InstanceBuffer = (FSlateUpdatableInstanceBuffer*)RenderBatch.InstanceData;
								InstanceBuffer->BindStreamSource(RHICmdList, 1, RenderBatch.InstanceOffset);

								// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
								if (!GRHISupportsBaseVertexIndex && !bAbsoluteIndices)
								{
									RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, RenderBatch.VertexOffset * sizeof(FSlateVertex));
									RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, InstanceCount);
								}
								else
								{
									uint32 VertexOffset = bAbsoluteIndices ? 0 : RenderBatch.VertexOffset; 
									RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, 0);
									RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, InstanceCount);
								}
							}
							//else
							//{
							//	for ( uint32 InstanceIndex = 0; InstanceIndex < InstanceCount; InstanceIndex++ )
							//	{
							//		FSlateUpdatableInstanceBuffer* InstanceBuffer = (FSlateUpdatableInstanceBuffer*)RenderBatch.InstanceData;
							//		InstanceBuffer->BindStreamSource(RHICmdList, 1, RenderBatch.InstanceOffset + InstanceIndex);

							//		// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
							//		if ( !GRHISupportsBaseVertexIndex )
							//		{
							//			RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, RenderBatch.VertexOffset * sizeof(FSlateVertex));
							//			RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							//		}
							//		else
							//		{
							//			RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), RenderBatch.VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							//		}
							//	}
							//}
						}
						else
						{
							RHICmdList.SetStreamSource(1, nullptr, 0);

							// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
							if ( !GRHISupportsBaseVertexIndex && !bAbsoluteIndices)
							{
								RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, RenderBatch.VertexOffset * sizeof(FSlateVertex));
								RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							}
							else
							{
								uint32 VertexOffset = bAbsoluteIndices ? 0 : RenderBatch.VertexOffset; 
								RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, 0);
								RHICmdList.DrawIndexedPrimitive(IndexBuffer->IndexBufferRHI, GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType), VertexOffset, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);
							}
						}
					}
				}
			}
			else if(ShaderType == ESlateShader::PostProcess)
			{
				SLATE_DRAW_EVENT(RHICmdList, PostProcess);

				const FVector4& QuadPositionData = ShaderParams.PixelParams;

				FPostProcessRectParams RectParams;
				RectParams.SourceTexture = BackBuffer.GetRenderTargetTexture();
				RectParams.SourceRect = FSlateRect(0,0,BackBuffer.GetSizeXY().X, BackBuffer.GetSizeXY().Y);
				RectParams.DestRect = FSlateRect(QuadPositionData.X, QuadPositionData.Y, QuadPositionData.Z, QuadPositionData.W);
				RectParams.SourceTextureSize = BackBuffer.GetSizeXY();

				RectParams.RestoreStateFunc = [&](FGraphicsPipelineStateInitializer& InGraphicsPSOInit) {
					UpdateScissorRect(InGraphicsPSOInit, Options.ViewProjectionMatrix, true);
				};

				RectParams.RestoreStateFuncPostPipelineState = [&]() {
					RHICmdList.SetStencilRef(StencilRef);
				};

				FBlurRectParams BlurParams;
				BlurParams.KernelSize = ShaderParams.PixelParams2.X;
				BlurParams.Strength = ShaderParams.PixelParams2.Y;
				BlurParams.DownsampleAmount = ShaderParams.PixelParams2.Z;

				PostProcessor->BlurRect(RHICmdList, RendererModule, BlurParams, RectParams);
			}
		}
		else
		{
			TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer = RenderBatch.CustomDrawer.Pin();
			if (CustomDrawer.IsValid())
			{
				SLATE_DRAW_EVENT(RHICmdList, CustomDrawer);

				// Disable scissor rect. A previous draw element may have had one
				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
				LastClippingIndex = -1;

				// This element is custom and has no Slate geometry.  Tell it to render itself now
				CustomDrawer->DrawRenderThread(RHICmdList, &BackBuffer.GetRenderTargetTexture());

				//We reset the maskingID here because otherwise the RT might not get re-set in the lines above see: if (bClearStencil || bForceStateChange)
				MaskingID = 0;
			
				// Something may have messed with the viewport size so set it back to the full target.
				RHICmdList.SetViewport( 0,0,0,BackBuffer.GetSizeXY().X, BackBuffer.GetSizeXY().Y, 0.0f ); 
				RHICmdList.SetStreamSource(0, VertexBuffer->VertexBufferRHI, 0);
			}
		}
	}

	for (int i = 0; i < NumScenes; i++)
	{
		// Don't need to delete SceneViews b/c the SceneViewFamily will delete it when it goes away.
		delete SceneViewFamilyContexts[i];
	}

	SceneViews.Empty();
	SceneViewFamilyContexts.Empty();

	INC_DWORD_STAT_BY(STAT_SlateScissorClips, ScissorClips);
	INC_DWORD_STAT_BY(STAT_SlateStencilClips, StencilClips);
}

ETextureSamplerFilter FSlateRHIRenderingPolicy::GetSamplerFilter(const TArray<FTextureLODGroup>& TextureLODGroups, const UTexture* Texture) const
{
	// Default to point filtering.
	ETextureSamplerFilter Filter = ETextureSamplerFilter::Point;

	switch (Texture->Filter)
	{
	case TF_Nearest: Filter = ETextureSamplerFilter::Point; break;
	case TF_Bilinear: Filter = ETextureSamplerFilter::Bilinear; break;
	case TF_Trilinear: Filter = ETextureSamplerFilter::Trilinear; break;

		// TF_Default
	default:
		// Use LOD group value to find proper filter setting.
		if (Texture->LODGroup < TextureLODGroups.Num())
		{
			Filter = TextureLODGroups[Texture->LODGroup].Filter;
		}
	}

	return Filter;
}

FSlateElementPS* FSlateRHIRenderingPolicy::GetTexturePixelShader( TShaderMap<FGlobalShaderType>* ShaderMap, ESlateShader::Type ShaderType, ESlateDrawEffect DrawEffects )
{
	FSlateElementPS* PixelShader = nullptr;

#if WITH_SLATE_VISUALIZERS
	if ( CVarShowSlateOverdraw.GetValueOnRenderThread() != 0 )
	{
		PixelShader = *TShaderMapRef<FSlateDebugOverdrawPS>(ShaderMap);
	}
	else
#endif
	{
	const bool bDrawDisabled = EnumHasAllFlags( DrawEffects, ESlateDrawEffect::DisabledEffect );
	const bool bUseTextureAlpha = !EnumHasAllFlags( DrawEffects, ESlateDrawEffect::IgnoreTextureAlpha );

	if ( bDrawDisabled )
	{
		switch ( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Border:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, true, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, true, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Font:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Font, true> >(ShaderMap);
			break;
		case ESlateShader::LineSegment:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::LineSegment, true> >(ShaderMap);
			break;
		}
	}
	else
	{
		switch ( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Border:
			if ( bUseTextureAlpha )
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, false, true> >(ShaderMap);
			}
			else
			{
				PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Border, false, false> >(ShaderMap);
			}
			break;
		case ESlateShader::Font:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::Font, false> >(ShaderMap);
			break;
		case ESlateShader::LineSegment:
			PixelShader = *TShaderMapRef<TSlateElementPS<ESlateShader::LineSegment, false> >(ShaderMap);
			break;
		}
	}
	}

#undef PixelShaderLookupTable

	return PixelShader;
}

FSlateMaterialShaderPS* FSlateRHIRenderingPolicy::GetMaterialPixelShader( const FMaterial* Material, ESlateShader::Type ShaderType, ESlateDrawEffect DrawEffects )
{
	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();

	const bool bDrawDisabled = EnumHasAllFlags(DrawEffects, ESlateDrawEffect::DisabledEffect);
	const bool bUseTextureAlpha = !EnumHasAllFlags(DrawEffects, ESlateDrawEffect::IgnoreTextureAlpha);

	FShader* FoundShader = nullptr;
	switch (ShaderType)
	{
	case ESlateShader::Default:
		if (bDrawDisabled)
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Default, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Default, false>::StaticType);
		}
		break;
	case ESlateShader::Border:
		if (bDrawDisabled)
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Border, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Border, false>::StaticType);
		}
		break;
	case ESlateShader::Font:
		if(bDrawDisabled)
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Font, true>::StaticType);
		}
		else
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Font, false>::StaticType);
		}
		break;
	case ESlateShader::Custom:
		{
			FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderPS<ESlateShader::Custom, false>::StaticType);
		}
		break;
	default:
		checkf(false, TEXT("Unsupported Slate shader type for use with materials"));
		break;
	}

	return FoundShader ? (FSlateMaterialShaderPS*)FoundShader->GetShaderChecked() : nullptr;
}

FSlateMaterialShaderVS* FSlateRHIRenderingPolicy::GetMaterialVertexShader( const FMaterial* Material, bool bUseInstancing )
{
	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();

	FShader* FoundShader = nullptr;
	if( bUseInstancing )
	{
		FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderVS<true>::StaticType);
	}
	else
	{
		FoundShader = MaterialShaderMap->GetShader(&TSlateMaterialShaderVS<false>::StaticType);
	}
	
	return FoundShader ? (FSlateMaterialShaderVS*)FoundShader->GetShaderChecked() : nullptr;
}

EPrimitiveType FSlateRHIRenderingPolicy::GetRHIPrimitiveType(ESlateDrawPrimitive::Type SlateType)
{
	switch(SlateType)
	{
	case ESlateDrawPrimitive::LineList:
		return PT_LineList;
	case ESlateDrawPrimitive::TriangleList:
	default:
		return PT_TriangleList;
	}

};


void FSlateRHIRenderingPolicy::AddSceneAt(FSceneInterface* Scene, int32 Index)
{
	ResourceManager->AddSceneAt(Scene, Index);
}

void FSlateRHIRenderingPolicy::ClearScenes()
{
	ResourceManager->ClearScenes();
}

void FSlateRHIRenderingPolicy::FlushGeneratedResources()
{
	PostProcessor->ReleaseRenderTargets();
}
	
