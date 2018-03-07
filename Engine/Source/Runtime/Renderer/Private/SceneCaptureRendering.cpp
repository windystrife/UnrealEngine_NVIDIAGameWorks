// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	
=============================================================================*/

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Misc/MemStack.h"
#include "EngineDefines.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "Engine/Scene.h"
#include "SceneInterface.h"
#include "GameFramework/Actor.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "Shader.h"
#include "TextureResource.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
// WaveWorks Start
#include "Components/WaveWorksShorelineCaptureComponent.h"
// WaveWorks End
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "MobileSceneCaptureRendering.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "RendererModule.h"

const TCHAR* GShaderSourceModeDefineName[] =
{
	TEXT("SOURCE_MODE_SCENE_COLOR_AND_OPACITY"),
	TEXT("SOURCE_MODE_SCENE_COLOR_NO_ALPHA"),
	nullptr,
	TEXT("SOURCE_MODE_SCENE_COLOR_SCENE_DEPTH"),
	TEXT("SOURCE_MODE_SCENE_DEPTH"),
	TEXT("SOURCE_MODE_DEVICE_DEPTH"),
	TEXT("SOURCE_MODE_NORMAL"),
	TEXT("SOURCE_MODE_BASE_COLOR")
};

/**
 * A pixel shader for capturing a component of the rendered scene for a scene capture.
 */
template<ESceneCaptureSource CaptureSource>
class TSceneCapturePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TSceneCapturePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		const TCHAR* DefineName = GShaderSourceModeDefineName[CaptureSource];
		if (DefineName)
		{
			OutEnvironment.SetDefine(DefineName, 1);
		}
	}

	TSceneCapturePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
	}
	TSceneCapturePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, GetPixelShader(), View, MD_PostProcess);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_SHADER_TYPE(template<>, TSceneCapturePS<SCS_SceneColorHDR>, TEXT("/Engine/Private/SceneCapturePixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TSceneCapturePS<SCS_SceneColorHDRNoAlpha>, TEXT("/Engine/Private/SceneCapturePixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TSceneCapturePS<SCS_SceneColorSceneDepth>,TEXT("/Engine/Private/SceneCapturePixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TSceneCapturePS<SCS_SceneDepth>,TEXT("/Engine/Private/SceneCapturePixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TSceneCapturePS<SCS_DeviceDepth>, TEXT("/Engine/Private/SceneCapturePixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TSceneCapturePS<SCS_Normal>,TEXT("/Engine/Private/SceneCapturePixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TSceneCapturePS<SCS_BaseColor>,TEXT("/Engine/Private/SceneCapturePixelShader.usf"),TEXT("Main"),SF_Pixel);

void FDeferredShadingSceneRenderer::CopySceneCaptureComponentToTarget(FRHICommandListImmediate& RHICmdList)
{
	ESceneCaptureSource SceneCaptureSource = ViewFamily.SceneCaptureSource;

	if (IsAnyForwardShadingEnabled(ViewFamily.GetShaderPlatform()) && (SceneCaptureSource == SCS_Normal || SceneCaptureSource == SCS_BaseColor))
	{
		SceneCaptureSource = SCS_SceneColorHDR;
	}

	if (SceneCaptureSource != SCS_FinalColorLDR)
	{
		SCOPED_DRAW_EVENT(RHICmdList, CaptureSceneComponent);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			FRHIRenderTargetView ColorView(ViewFamily.RenderTarget->GetRenderTargetTexture(), 0, -1, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::EStore);
			FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView());
			RHICmdList.SetRenderTargetsAndClear(Info);
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			if (SceneCaptureSource == SCS_SceneColorHDR && ViewFamily.SceneCaptureCompositeMode == SCCM_Composite)
			{
				// Blend with existing render target color. Scene capture color is already pre-multiplied by alpha.
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
			}
			else if (SceneCaptureSource == SCS_SceneColorHDR && ViewFamily.SceneCaptureCompositeMode == SCCM_Additive)
			{
				// Add to existing render target color. Scene capture color is already pre-multiplied by alpha.
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
			}
			else
			{
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			}

			TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			if (SceneCaptureSource == SCS_SceneColorHDR)
			{
				TShaderMapRef<TSceneCapturePS<SCS_SceneColorHDR> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);
			}
			else if (SceneCaptureSource == SCS_SceneColorHDRNoAlpha)
			{
				TShaderMapRef<TSceneCapturePS<SCS_SceneColorHDRNoAlpha> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);
			}
			else if (SceneCaptureSource == SCS_SceneColorSceneDepth)
			{
				TShaderMapRef<TSceneCapturePS<SCS_SceneColorSceneDepth> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);
			}
			else if (SceneCaptureSource == SCS_SceneDepth)
			{
				TShaderMapRef<TSceneCapturePS<SCS_SceneDepth> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);
			}
			else if (ViewFamily.SceneCaptureSource == SCS_DeviceDepth)
			{
				TShaderMapRef<TSceneCapturePS<SCS_DeviceDepth> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);
			}
			else if (SceneCaptureSource == SCS_Normal)
			{
				TShaderMapRef<TSceneCapturePS<SCS_Normal> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);
			}
			else if (SceneCaptureSource == SCS_BaseColor)
			{
				TShaderMapRef<TSceneCapturePS<SCS_BaseColor> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);
			}
			else
			{
				check(0);
			}

			VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);

			DrawRectangle(
				RHICmdList,
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.UnconstrainedViewRect.Size(),
				FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}
	}
}

static void UpdateSceneCaptureContentDeferred_RenderThread(
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderer* SceneRenderer, 
	FRenderTarget* RenderTarget, 
	FTexture* RenderTargetTexture, 
	const FName OwnerName, 
	const FResolveParams& ResolveParams)
{
	FMemMark MemStackMark(FMemStack::Get());

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);
	{
#if WANTS_DRAW_MESH_EVENTS
		FString EventName;
		OwnerName.ToString(EventName);
		SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("SceneCapture %s"), *EventName);
#else
		SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneCaptureContent_RenderThread);
#endif

		const FRenderTarget* Target = SceneRenderer->ViewFamily.RenderTarget;

		FViewInfo& View = SceneRenderer->Views[0];
		FIntRect ViewRect = View.ViewRect;
		FIntRect UnconstrainedViewRect = View.UnconstrainedViewRect;
		SetRenderTarget(RHICmdList, Target->GetRenderTargetTexture(), nullptr, true);
		DrawClearQuad(RHICmdList, true, FLinearColor::Black, false, 0, false, 0, Target->GetSizeXY(), ViewRect);

		// Render the scene normally
		{
			SCOPED_DRAW_EVENT(RHICmdList, RenderScene);
			SceneRenderer->Render(RHICmdList);
		}

		// Note: When the ViewFamily.SceneCaptureSource requires scene textures (i.e. SceneCaptureSource != SCS_FinalColorLDR), the copy to RenderTarget 
		// will be done in CopySceneCaptureComponentToTarget while the GBuffers are still alive for the frame.
		RHICmdList.CopyToResolveTarget(RenderTarget->GetRenderTargetTexture(), RenderTargetTexture->TextureRHI, false, ResolveParams);
	}

	FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);
}

static void UpdateSceneCaptureContent_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FSceneRenderer* SceneRenderer,
	FRenderTarget* RenderTarget,
	FTexture* RenderTargetTexture,
	const FName OwnerName,
	const FResolveParams& ResolveParams)
{
	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

	switch (SceneRenderer->Scene->GetShadingPath())
	{
		case EShadingPath::Mobile:
		{
			UpdateSceneCaptureContentMobile_RenderThread(
				RHICmdList,
				SceneRenderer,
				RenderTarget,
				RenderTargetTexture,
				OwnerName,
				ResolveParams);
			break;
		}
		case EShadingPath::Deferred:
		{
			UpdateSceneCaptureContentDeferred_RenderThread(
				RHICmdList,
				SceneRenderer,
				RenderTarget,
				RenderTargetTexture,
				OwnerName,
				ResolveParams);
			break;
		}
		default:
			checkNoEntry();
			break;
		}
}

void BuildProjectionMatrix(FIntPoint RenderTargetSize, ECameraProjectionMode::Type ProjectionType, float FOV, float InOrthoWidth, FMatrix& ProjectionMatrix)
{
	float XAxisMultiplier;
	float YAxisMultiplier;

	if (RenderTargetSize.X > RenderTargetSize.Y)
	{
		// if the viewport is wider than it is tall
		XAxisMultiplier = 1.0f;
		YAxisMultiplier = RenderTargetSize.X / (float)RenderTargetSize.Y;
	}
	else
	{
		// if the viewport is taller than it is wide
		XAxisMultiplier = RenderTargetSize.Y / (float)RenderTargetSize.X;
		YAxisMultiplier = 1.0f;
	}

	if (ProjectionType == ECameraProjectionMode::Orthographic)
	{
		check((int32)ERHIZBuffer::IsInverted);
		const float OrthoWidth = InOrthoWidth / 2.0f;
		const float OrthoHeight = InOrthoWidth / 2.0f * XAxisMultiplier / YAxisMultiplier;

		const float NearPlane = 0;
		const float FarPlane = WORLD_MAX / 8.0f;

		const float ZScale = 1.0f / (FarPlane - NearPlane);
		const float ZOffset = -NearPlane;

		ProjectionMatrix = FReversedZOrthoMatrix(
			OrthoWidth,
			OrthoHeight,
			ZScale,
			ZOffset
			);
	}
	else
	{
		if ((int32)ERHIZBuffer::IsInverted)
		{
			ProjectionMatrix = FReversedZPerspectiveMatrix(
				FOV,
				FOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
				);
		}
		else
		{
			ProjectionMatrix = FPerspectiveMatrix(
				FOV,
				FOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
				);
		}
	}
}

FSceneRenderer* CreateSceneRendererForSceneCapture(
	FScene* Scene,
	USceneCaptureComponent* SceneCaptureComponent,
	FRenderTarget* RenderTarget,
	FIntPoint RenderTargetSize,
	const TArrayView<const FSceneCaptureViewInfo> Views,
	float MaxViewDistance,
	bool bCaptureSceneColor,
	bool bIsPlanarReflection,
	FPostProcessSettings* PostProcessSettings,
	float PostProcessBlendWeight,
	const AActor* ViewActor)
{
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		RenderTarget,
		Scene,
		SceneCaptureComponent->ShowFlags)
		.SetResolveScene(!bCaptureSceneColor)
		.SetRealtimeUpdate(bIsPlanarReflection || SceneCaptureComponent->bCaptureEveryFrame || SceneCaptureComponent->bAlwaysPersistRenderingState));

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FSceneCaptureViewInfo& SceneCaptureViewInfo = Views[ViewIndex];

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.SetViewRectangle(SceneCaptureViewInfo.ViewRect);
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.ViewActor = ViewActor;
		ViewInitOptions.ViewOrigin = SceneCaptureViewInfo.ViewLocation;
		ViewInitOptions.ViewRotationMatrix = SceneCaptureViewInfo.ViewRotationMatrix;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverrideFarClippingPlaneDistance = MaxViewDistance;
		ViewInitOptions.StereoPass = SceneCaptureViewInfo.StereoPass;
		ViewInitOptions.SceneViewStateInterface = SceneCaptureComponent->GetViewState(ViewIndex);
		ViewInitOptions.ProjectionMatrix = SceneCaptureViewInfo.ProjectionMatrix;
		ViewInitOptions.LODDistanceFactor = FMath::Clamp(SceneCaptureComponent->LODDistanceFactor, .01f, 100.0f);

		if (bCaptureSceneColor)
		{
			ViewFamily.EngineShowFlags.PostProcessing = 0;
			ViewInitOptions.OverlayColor = FLinearColor::Black;
		}

		FSceneView* View = new FSceneView(ViewInitOptions);

		View->bIsSceneCapture = true;
		// Note: this has to be set before EndFinalPostprocessSettings
		View->bIsPlanarReflection = bIsPlanarReflection;
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		View->bEnableVxgiForSceneCapture = SceneCaptureComponent->bEnableVxgi;
#endif
		// NVCHANGE_END: Add VXGI

		check(SceneCaptureComponent);
		for (auto It = SceneCaptureComponent->HiddenComponents.CreateConstIterator(); It; ++It)
		{
			// If the primitive component was destroyed, the weak pointer will return NULL.
			UPrimitiveComponent* PrimitiveComponent = It->Get();
			if (PrimitiveComponent)
			{
				View->HiddenPrimitives.Add(PrimitiveComponent->ComponentId);
			}
		}

		for (auto It = SceneCaptureComponent->HiddenActors.CreateConstIterator(); It; ++It)
		{
			AActor* Actor = *It;

			if (Actor)
			{
				TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
				Actor->GetComponents(PrimitiveComponents);
				for (int32 ComponentIndex = 0; ComponentIndex < PrimitiveComponents.Num(); ++ComponentIndex)
				{
					View->HiddenPrimitives.Add(PrimitiveComponents[ComponentIndex]->ComponentId);
				}
			}
		}

		if (SceneCaptureComponent->PrimitiveRenderMode == ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList)
		{
			View->ShowOnlyPrimitives.Emplace();

			for (auto It = SceneCaptureComponent->ShowOnlyComponents.CreateConstIterator(); It; ++It)
			{
				// If the primitive component was destroyed, the weak pointer will return NULL.
				UPrimitiveComponent* PrimitiveComponent = It->Get();
				if (PrimitiveComponent)
				{
					View->ShowOnlyPrimitives->Add(PrimitiveComponent->ComponentId);
				}
			}

			for (auto It = SceneCaptureComponent->ShowOnlyActors.CreateConstIterator(); It; ++It)
			{
				AActor* Actor = *It;

				if (Actor)
				{
					TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
					Actor->GetComponents(PrimitiveComponents);
					for (int32 ComponentIndex = 0; ComponentIndex < PrimitiveComponents.Num(); ++ComponentIndex)
					{
						View->ShowOnlyPrimitives->Add(PrimitiveComponents[ComponentIndex]->ComponentId);
					}
				}
			}
		}
		else if (SceneCaptureComponent->ShowOnlyComponents.Num() > 0 || SceneCaptureComponent->ShowOnlyActors.Num() > 0)
		{
			static bool bWarned = false;

			if (!bWarned)
			{
				UE_LOG(LogRenderer, Log, TEXT("Scene Capture has ShowOnlyComponents or ShowOnlyActors ignored by the PrimitiveRenderMode setting! %s"), *SceneCaptureComponent->GetPathName());
				bWarned = true;
			}
		}

		ViewFamily.Views.Add(View);

		View->StartFinalPostprocessSettings(SceneCaptureViewInfo.ViewLocation);
		View->OverridePostProcessSettings(*PostProcessSettings, PostProcessBlendWeight);
		View->EndFinalPostprocessSettings(ViewInitOptions);
	}

	return FSceneRenderer::CreateSceneRenderer(&ViewFamily, NULL);
}

FSceneRenderer* CreateSceneRendererForSceneCapture(
	FScene* Scene,
	USceneCaptureComponent* SceneCaptureComponent,
	FRenderTarget* RenderTarget,
	FIntPoint RenderTargetSize,
	const FMatrix& ViewRotationMatrix,
	const FVector& ViewLocation,
	const FMatrix& ProjectionMatrix,
	float MaxViewDistance,
	bool bCaptureSceneColor,
	bool bIsPlanarReflection,
	FPostProcessSettings* PostProcessSettings,
	float PostProcessBlendWeight,
	const AActor* ViewActor)
{
	FSceneCaptureViewInfo SceneCaptureViewInfo;
	SceneCaptureViewInfo.ViewRotationMatrix = ViewRotationMatrix;
	SceneCaptureViewInfo.ViewLocation = ViewLocation;
	SceneCaptureViewInfo.ProjectionMatrix = ProjectionMatrix;
	SceneCaptureViewInfo.StereoPass = EStereoscopicPass::eSSP_FULL;
	SceneCaptureViewInfo.ViewRect = FIntRect(0, 0, RenderTargetSize.X, RenderTargetSize.Y);
	
	return CreateSceneRendererForSceneCapture(
		Scene, 
		SceneCaptureComponent, 
		RenderTarget, 
		RenderTargetSize, 
		{ SceneCaptureViewInfo },
		MaxViewDistance, 
		bCaptureSceneColor, 
		bIsPlanarReflection, 
		PostProcessSettings, 
		PostProcessBlendWeight,
		ViewActor);
}

void FScene::UpdateSceneCaptureContents(USceneCaptureComponent2D* CaptureComponent)
{
	check(CaptureComponent);

	if (CaptureComponent->TextureTarget)
	{
		// Only ensure motion blur cache is up to date when doing USceneCaptureComponent2D::CaptureScene(),
		// but only when bAlwaysPersistRenderingState == true for backward compatibility.
		if (!CaptureComponent->bCaptureEveryFrame && CaptureComponent->bAlwaysPersistRenderingState)
		{
			// We assume the world is not paused since the CaptureScene() has manually been called.
			EnsureMotionBlurCacheIsUpToDate(/* bWorldIsPaused = */ false);
		}

		FTransform Transform = CaptureComponent->GetComponentToWorld();
		FVector ViewLocation = Transform.GetTranslation();

		// Remove the translation from Transform because we only need rotation.
		Transform.SetTranslation(FVector::ZeroVector);
		Transform.SetScale3D(FVector::OneVector);
		FMatrix ViewRotationMatrix = Transform.ToInverseMatrixWithScale();

		// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
		ViewRotationMatrix = ViewRotationMatrix * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));
		const float FOV = CaptureComponent->FOVAngle * (float)PI / 360.0f;
		FIntPoint CaptureSize(CaptureComponent->TextureTarget->GetSurfaceWidth(), CaptureComponent->TextureTarget->GetSurfaceHeight());

		FMatrix ProjectionMatrix;
		if (CaptureComponent->bUseCustomProjectionMatrix)
		{
			ProjectionMatrix = CaptureComponent->CustomProjectionMatrix;
		}
		else
		{
			BuildProjectionMatrix(CaptureSize, CaptureComponent->ProjectionType, FOV, CaptureComponent->OrthoWidth, ProjectionMatrix);
		}

		const bool bUseSceneColorTexture = CaptureComponent->CaptureSource != SCS_FinalColorLDR;

		FSceneRenderer* SceneRenderer = CreateSceneRendererForSceneCapture(
			this, 
			CaptureComponent, 
			CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource(), 
			CaptureSize, 
			ViewRotationMatrix, 
			ViewLocation, 
			ProjectionMatrix, 
			CaptureComponent->MaxViewDistanceOverride, 
			bUseSceneColorTexture, 
			false, 
			&CaptureComponent->PostProcessSettings, 
			CaptureComponent->PostProcessBlendWeight,
			CaptureComponent->GetViewOwner());

		SceneRenderer->ViewFamily.SceneCaptureSource = CaptureComponent->CaptureSource;
		SceneRenderer->ViewFamily.SceneCaptureCompositeMode = CaptureComponent->CompositeMode;

		{
			FPlane ClipPlane = FPlane(CaptureComponent->ClipPlaneBase, CaptureComponent->ClipPlaneNormal.GetSafeNormal());

			for (FSceneView& View : SceneRenderer->Views)
			{
				View.bCameraCut = CaptureComponent->bCameraCutThisFrame;

				if (CaptureComponent->bEnableClipPlane)
				{
					View.GlobalClippingPlane = ClipPlane;
					// Jitter can't be removed completely due to the clipping plane
					View.bAllowTemporalJitter = false;
				}
			}
		}

		// Reset scene capture's camera cut.
		CaptureComponent->bCameraCutThisFrame = false;

		FTextureRenderTargetResource* TextureRenderTarget = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();
		const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;
		ENQUEUE_RENDER_COMMAND(CaptureCommand)(
			[SceneRenderer, TextureRenderTarget, OwnerName](FRHICommandListImmediate& RHICmdList)
			{
				UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, TextureRenderTarget, OwnerName, FResolveParams());
			}
		);
	}
}

// WaveWorks Start
void FScene::UpdateSceneCaptureContents(UWaveWorksShorelineCaptureComponent* CaptureComponent)
{
	check(CaptureComponent);

	if (CaptureComponent->TextureTarget)
	{
		// Only ensure motion blur cache is up to date when doing USceneCaptureComponent2D::CaptureScene(),
		// but only when bAlwaysPersistRenderingState == true for backward compatibility.
		if (!CaptureComponent->bCaptureEveryFrame && CaptureComponent->bAlwaysPersistRenderingState)
		{
			// We assume the world is not paused since the CaptureScene() has manually been called.
			EnsureMotionBlurCacheIsUpToDate(/* bWorldIsPaused = */ false);
		}

		FTransform Transform = CaptureComponent->GetComponentToWorld();
		FVector ViewLocation = Transform.GetTranslation();

		// Remove the translation from Transform because we only need rotation.
		Transform.SetTranslation(FVector::ZeroVector);
		Transform.SetScale3D(FVector::OneVector);
		FMatrix ViewRotationMatrix = Transform.ToInverseMatrixWithScale();

		// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
		ViewRotationMatrix = ViewRotationMatrix * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));
		const float FOV = CaptureComponent->FOVAngle * (float)PI / 360.0f;
		FIntPoint CaptureSize(CaptureComponent->TextureTarget->GetSurfaceWidth(), CaptureComponent->TextureTarget->GetSurfaceHeight());

		FMatrix ProjectionMatrix;
		if (CaptureComponent->bUseCustomProjectionMatrix)
		{
			ProjectionMatrix = CaptureComponent->CustomProjectionMatrix;
		}
		else
		{
			BuildProjectionMatrix(CaptureSize, CaptureComponent->ProjectionType, FOV, CaptureComponent->OrthoWidth, ProjectionMatrix);
		}

		const bool bUseSceneColorTexture = true;

		FSceneRenderer* SceneRenderer = CreateSceneRendererForSceneCapture(
			this,
			CaptureComponent,
			CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource(),
			CaptureSize,
			ViewRotationMatrix,
			ViewLocation,
			ProjectionMatrix,
			CaptureComponent->MaxViewDistanceOverride,
			bUseSceneColorTexture,
			false,
			&CaptureComponent->PostProcessSettings,
			CaptureComponent->PostProcessBlendWeight,
			CaptureComponent->GetViewOwner());

		SceneRenderer->ViewFamily.SceneCaptureSource = CaptureComponent->CaptureSource;
		SceneRenderer->ViewFamily.SceneCaptureCompositeMode = CaptureComponent->CompositeMode;

		{
			FPlane ClipPlane = FPlane(CaptureComponent->ClipPlaneBase, CaptureComponent->ClipPlaneNormal.GetSafeNormal());

			for (FSceneView& View : SceneRenderer->Views)
			{
				View.bCameraCut = CaptureComponent->bCameraCutThisFrame;

				if (CaptureComponent->bEnableClipPlane)
				{
					View.GlobalClippingPlane = ClipPlane;
					// Jitter can't be removed completely due to the clipping plane
					View.bAllowTemporalJitter = false;
				}
			}
		}

		// Reset scene capture's camera cut.
		CaptureComponent->bCameraCutThisFrame = false;

		FTextureRenderTargetResource* TextureRenderTarget = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();
		const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;
		ENQUEUE_RENDER_COMMAND(CaptureCommand)(
			[SceneRenderer, TextureRenderTarget, OwnerName](FRHICommandListImmediate& RHICmdList)
		{
			UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, TextureRenderTarget, OwnerName, FResolveParams());
		}
		);
	}
}
// WaveWorks End

void FScene::UpdateSceneCaptureContents(USceneCaptureComponentCube* CaptureComponent)
{
	struct FLocal
	{
		/** Creates a transformation for a cubemap face, following the D3D cubemap layout. */
		static FMatrix CalcCubeFaceTransform(ECubeFace Face)
		{
			static const FVector XAxis(1.f, 0.f, 0.f);
			static const FVector YAxis(0.f, 1.f, 0.f);
			static const FVector ZAxis(0.f, 0.f, 1.f);

			// vectors we will need for our basis
			FVector vUp(YAxis);
			FVector vDir;
			switch (Face)
			{
				case CubeFace_PosX:
					vDir = XAxis;
					break;
				case CubeFace_NegX:
					vDir = -XAxis;
					break;
				case CubeFace_PosY:
					vUp = -ZAxis;
					vDir = YAxis;
					break;
				case CubeFace_NegY:
					vUp = ZAxis;
					vDir = -YAxis;
					break;
				case CubeFace_PosZ:
					vDir = ZAxis;
					break;
				case CubeFace_NegZ:
					vDir = -ZAxis;
					break;
			}
			// derive right vector
			FVector vRight(vUp ^ vDir);
			// create matrix from the 3 axes
			return FBasisVectorMatrix(vRight, vUp, vDir, FVector::ZeroVector);
		}
	} ;

	check(CaptureComponent);

	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4 && CaptureComponent->TextureTarget)
	{
		const float FOV = 90 * (float)PI / 360.0f;
		for (int32 faceidx = 0; faceidx < (int32)ECubeFace::CubeFace_MAX; faceidx++)
		{
			const ECubeFace TargetFace = (ECubeFace)faceidx;
			const FVector Location = CaptureComponent->GetComponentToWorld().GetTranslation();
			const FMatrix ViewRotationMatrix = FLocal::CalcCubeFaceTransform(TargetFace);
			FIntPoint CaptureSize(CaptureComponent->TextureTarget->GetSurfaceWidth(), CaptureComponent->TextureTarget->GetSurfaceHeight());
			FMatrix ProjectionMatrix;
			BuildProjectionMatrix(CaptureSize, ECameraProjectionMode::Perspective, FOV, 1.0f, ProjectionMatrix);
			FPostProcessSettings PostProcessSettings;

			FSceneRenderer* SceneRenderer = CreateSceneRendererForSceneCapture(this, CaptureComponent, CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource(), CaptureSize, ViewRotationMatrix, Location, ProjectionMatrix, CaptureComponent->MaxViewDistanceOverride, true, false, &PostProcessSettings, 0, CaptureComponent->GetViewOwner());
			SceneRenderer->ViewFamily.SceneCaptureSource = SCS_SceneColorHDR;

			FTextureRenderTargetCubeResource* TextureRenderTarget = static_cast<FTextureRenderTargetCubeResource*>(CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource());
			const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;
			ENQUEUE_RENDER_COMMAND(CaptureCommand)(
				[SceneRenderer, TextureRenderTarget, OwnerName, TargetFace](FRHICommandListImmediate& RHICmdList)
				{
					UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, TextureRenderTarget, OwnerName, FResolveParams(FResolveRect(), TargetFace));
				}
			);
		}
	}
}
