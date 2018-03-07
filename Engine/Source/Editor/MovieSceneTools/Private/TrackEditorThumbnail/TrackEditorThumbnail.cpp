// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditorThumbnail/TrackEditorThumbnail.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "ShowFlags.h"
#include "RHIStaticStates.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Slate/SlateTextures.h"
#include "Slate/SceneViewport.h"
#include "GlobalShader.h"
#include "ScreenRendering.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "PipelineStateCache.h"

namespace TrackEditorThumbnailConstants
{
	const double ThumbnailFadeInDuration = 0.25f;
}


class FThumbnailViewportClient
	: public FLevelEditorViewportClient
{
public:

	FThumbnailViewportClient() : FLevelEditorViewportClient(nullptr) {}

	float CurrentWorldTime, DeltaWorldTime;

	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const EStereoscopicPass StereoPass = eSSP_FULL) override
	{
		FSceneView* View = FLevelEditorViewportClient::CalcSceneView(ViewFamily, StereoPass);

		// Artificially set the world times so that graphics settings apply correctly (we don't tick the world when rendering thumbnails)
		ViewFamily->CurrentWorldTime = CurrentWorldTime;
		ViewFamily->DeltaWorldTime = DeltaWorldTime;

		View->FinalPostProcessSettings.bOverride_AutoExposureSpeedDown = View->FinalPostProcessSettings.bOverride_AutoExposureSpeedUp = true;
		View->FinalPostProcessSettings.AutoExposureSpeedDown = View->FinalPostProcessSettings.AutoExposureSpeedUp = 0.02f;
		return View;
	}

};


/* FTrackEditorThumbnail structors
 *****************************************************************************/

FTrackEditorThumbnail::FTrackEditorThumbnail(const FOnThumbnailDraw& InOnDraw, const FIntPoint& InSize, TRange<float> InTimeRange, float InPosition)
	: OnDraw(InOnDraw)
	, Size(InSize)
	, Texture(nullptr)
	, TimeRange(InTimeRange)
	, Position(InPosition)
	, FadeInCurve(0.0f, TrackEditorThumbnailConstants::ThumbnailFadeInDuration)
{
	SortOrder = 0;
}


FTrackEditorThumbnail::~FTrackEditorThumbnail()
{
	if (Texture && !bHasFinishedDrawing)
	{
		FlushRenderingCommands();
	}
	DestroyTexture();
}


void FTrackEditorThumbnail::DestroyTexture()
{
	if (Texture)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(DestroyTexture,
			FSlateTexture2DRHIRef*, Texture, Texture,
		{
			Texture->ReleaseResource();
			delete Texture;
		});

		Texture = nullptr;
	}
}


/* FTrackEditorThumbnail interface
 *****************************************************************************/

void FTrackEditorThumbnail::CopyTextureIn(TSharedPtr<FSceneViewport> SceneViewport)
{
	// Note: We explicitly capture the viewport scene here to ensure that the render target lives as long as this render command.
	// This means we don't have to flush the rendering commands all the time
	SceneViewportReference = SceneViewport;

	FSlateRenderTargetRHI* RenderTarget = (FSlateRenderTargetRHI*)SceneViewport->GetViewportRenderTargetTexture();
	if (RenderTarget)
	{
		CopyTextureIn(RenderTarget->GetRHIRef());
	}
}

void FTrackEditorThumbnail::CopyTextureIn(FTexture2DRHIRef SourceTexture)
{
	// This code is a little manual because CopyToResolveTarget on its own can't resolve a sub rect without offsetting it inside the destination texture
	// So we render our own rectangle onto a render target of the right size, so we can maintain the correct aspect ratio and fov settings of the camera,
	// but still fulfil the desired thumbnail size.
	FSlateTexture2DRHIRef* TargetTexture = Texture;

	static const FName RendererModuleName( "Renderer" );
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);
	FThreadSafeBool* bHasFinishedDrawingPtr = &bHasFinishedDrawing;

	auto RenderCommand = [TargetTexture, RendererModule, SourceTexture, bHasFinishedDrawingPtr](FRHICommandListImmediate& RHICmdList){

		const FIntPoint TargetSize(TargetTexture->GetWidth(), TargetTexture->GetHeight());

		FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
			TargetSize,
			PF_B8G8R8A8,
			FClearValueBinding::None,
			TexCreate_None,
			TexCreate_RenderTargetable,
			false);

		TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
		RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("ResampleTexture"));
		check(ResampleTexturePooledRenderTarget);

		const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);
//		RHICmdList.ClearColorTexture(DestRenderTarget.TargetableTexture, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));

		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_Always>::GetRHI();

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
		
		TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SourceTexture);

		const float Scale = FMath::Min(float(SourceTexture->GetSizeX()) / TargetTexture->GetWidth(), float(SourceTexture->GetSizeY()) / TargetTexture->GetHeight());
		const float Left = (SourceTexture->GetSizeX() - TargetTexture->GetWidth()*Scale) * .5f;
		const float Top = (SourceTexture->GetSizeY() - TargetTexture->GetHeight()*Scale) * .5f;

		const float U = Left / float(SourceTexture->GetSizeX());
		const float V = Top / float(SourceTexture->GetSizeY());
		const float SizeU = float(TargetTexture->GetWidth() * Scale) / float(SourceTexture->GetSizeX());
		const float SizeV = float(TargetTexture->GetHeight() * Scale) / float(SourceTexture->GetSizeY());

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,									// Dest X, Y
			TargetSize.X,							// Dest Width
			TargetSize.Y,							// Dest Height
			U, V,									// Source U, V
			SizeU, SizeV,							// Source USize, VSize
			TargetSize,								// Target buffer size
			FIntPoint(1, 1),						// Source texture size
			*VertexShader,
			EDRF_Default);

		// Asynchronously copy render target from GPU to CPU
		const bool bKeepOriginalSurface = false;
		RHICmdList.CopyToResolveTarget(
			DestRenderTarget.TargetableTexture,
			TargetTexture->GetTypedResource(),
			bKeepOriginalSurface,
			FResolveParams());

		*bHasFinishedDrawingPtr = true;
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ResolveCaptureFrameTexture,
		TFunction<void(FRHICommandListImmediate&)>, InRenderCommand, RenderCommand,
	{
		InRenderCommand(RHICmdList);
	});
}


void FTrackEditorThumbnail::DrawThumbnail()
{
	// Delay texture creation until we actually draw the thumbnail
	if (Size.X > 1 && Size.Y > 1)
	{
		DestroyTexture();
		Texture = new FSlateTexture2DRHIRef(Size.X, Size.Y, PF_B8G8R8A8, nullptr, TexCreate_Dynamic, true);
		BeginInitResource(Texture);
	}

	OnDraw.ExecuteIfBound(*this);
}


void FTrackEditorThumbnail::SetupFade(const TSharedRef<SWidget>& InWidget)
{
	FadeInCurve.PlayReverse(InWidget);
	FadeInCurve.Pause();
}


void FTrackEditorThumbnail::PlayFade()
{
	FadeInCurve.Resume();
}


float FTrackEditorThumbnail::GetFadeInCurve() const 
{
	return FadeInCurve.GetLerp();
}


/* ISlateViewport interface
 *****************************************************************************/

FIntPoint FTrackEditorThumbnail::GetSize() const
{
	return Size;
}


FSlateShaderResource* FTrackEditorThumbnail::GetViewportRenderTargetTexture() const
{
	return Texture;
}


bool FTrackEditorThumbnail::RequiresVsync() const 
{
	return false;
}


FTrackEditorThumbnailCache::FTrackEditorThumbnailCache(const TSharedPtr<FTrackEditorThumbnailPool>& InThumbnailPool, IViewportThumbnailClient* InViewportThumbnailClient)
	: ViewportThumbnailClient(InViewportThumbnailClient)
	, CustomThumbnailClient(nullptr)
	, ThumbnailPool(InThumbnailPool)
{
	check(ViewportThumbnailClient);

	FrameCount = 0;
	LastComputationTime = 0;
	bForceRedraw = false;
	bNeedsNewThumbnails = false;
}


FTrackEditorThumbnailCache::FTrackEditorThumbnailCache(const TSharedPtr<FTrackEditorThumbnailPool>& InThumbnailPool, ICustomThumbnailClient* InCustomThumbnailClient)
	: ViewportThumbnailClient(nullptr)
	, CustomThumbnailClient(InCustomThumbnailClient)
	, ThumbnailPool(InThumbnailPool)
{
	check(CustomThumbnailClient);

	FrameCount = 0;
	LastComputationTime = 0;
	bForceRedraw = false;
	bNeedsNewThumbnails = false;
}


FTrackEditorThumbnailCache::~FTrackEditorThumbnailCache()
{
	TSharedPtr<FTrackEditorThumbnailPool> PinnedPool = ThumbnailPool.Pin();
	if (PinnedPool.IsValid())
	{
		PinnedPool->RemoveThumbnailsNeedingRedraw(Thumbnails);
	}

	if (InternalViewportClient.IsValid())
	{
		InternalViewportClient->Viewport = nullptr;
	}
}


void FTrackEditorThumbnailCache::SetSingleReferenceFrame(TOptional<float> InReferenceFrame)
{
	CurrentCache.SingleReferenceFrame = InReferenceFrame;
}

void FTrackEditorThumbnailCache::Update(const TRange<float>& NewRange, const TRange<float>& VisibleRange, const FIntPoint& AllottedSize, const FIntPoint& InDesiredSize, EThumbnailQuality Quality, double InCurrentTime)
{
	PreviousCache.TimeRange = CurrentCache.TimeRange;
	PreviousCache.VisibleRange = CurrentCache.VisibleRange;
	PreviousCache.AllottedSize = CurrentCache.AllottedSize;
	PreviousCache.DesiredSize = CurrentCache.DesiredSize;
	PreviousCache.Quality = CurrentCache.Quality;

	CurrentCache.TimeRange = NewRange;
	CurrentCache.VisibleRange = VisibleRange;
	CurrentCache.AllottedSize = AllottedSize;
	CurrentCache.DesiredSize = InDesiredSize;
	CurrentCache.Quality = Quality;

	Revalidate(InCurrentTime);

	// Only update the single reference frame value once we've updated, since that can get set at any time, but Update() may be throttled
	PreviousCache.SingleReferenceFrame = CurrentCache.SingleReferenceFrame;
}


FIntPoint FTrackEditorThumbnailCache::CalculateTextureSize() const
{
	UCameraComponent* CameraComponent = InternalViewportClient->GetCameraComponentForView();
	float DesiredRatio = CameraComponent ? CameraComponent->AspectRatio : 1.77f;

	if (CurrentCache.DesiredSize.X <= 0 || CurrentCache.DesiredSize.Y <= 0)
	{
		return FIntPoint(0,0);
	}

	float SizeRatio = float(CurrentCache.DesiredSize.X) / CurrentCache.DesiredSize.Y;

	float X = CurrentCache.DesiredSize.X;
	float Y = CurrentCache.DesiredSize.Y;

	if (SizeRatio > DesiredRatio)
	{
		// Take width
		Y = CurrentCache.DesiredSize.X / DesiredRatio;
	}
	else if (SizeRatio < DesiredRatio)
	{
		// Take height
		X = CurrentCache.DesiredSize.Y * DesiredRatio;
	}

	float Scale;
	switch (CurrentCache.Quality)
	{
		case EThumbnailQuality::Draft: 	Scale = 0.5f; 	break;
		case EThumbnailQuality::Best: 	Scale = 2.f; 	break;
		default: 						Scale = 1.f; 	break;
}

	return FIntPoint(
		FMath::RoundToInt(X * Scale),
		FMath::RoundToInt(Y * Scale)
		);
}

bool FTrackEditorThumbnailCache::ShouldRegenerateEverything() const
{
	if (bForceRedraw)
	{
		return true;
	}

	const float PreviousScale = PreviousCache.TimeRange.Size<float>() / PreviousCache.AllottedSize.X;
	const float CurrentScale = CurrentCache.TimeRange.Size<float>() / CurrentCache.AllottedSize.X;
	const float Threshold = PreviousScale * 0.01f;

	return PreviousCache.DesiredSize != CurrentCache.DesiredSize || !FMath::IsNearlyEqual(PreviousScale, CurrentScale, Threshold);
}


void FTrackEditorThumbnailCache::DrawViewportThumbnail(FTrackEditorThumbnail& TrackEditorThumbnail)
{
	if (CustomThumbnailClient)
	{
		CustomThumbnailClient->Draw(TrackEditorThumbnail);
	}
	else if (InternalViewportScene.IsValid())
	{
		check(ViewportThumbnailClient);

		// Ask the client to setup the frame
		ViewportThumbnailClient->PreDraw(TrackEditorThumbnail, *InternalViewportClient, *InternalViewportScene);
		
		// Finalize the view
		InternalViewportClient->bLockedCameraView = true;
		InternalViewportClient->UpdateViewForLockedActor();
		InternalViewportClient->GetWorld()->SendAllEndOfFrameUpdates();

		// Update the viewport RHI if necessary
		FIntPoint Size = CalculateTextureSize();
		if (InternalViewportScene->GetSize() != Size)
		{
			InternalViewportScene->UpdateViewportRHI(false, Size.X, Size.Y, EWindowMode::Windowed, PF_Unknown);
		}

		InternalViewportClient->DeltaWorldTime = TrackEditorThumbnail.GetEvalPosition() - InternalViewportClient->CurrentWorldTime;
		InternalViewportClient->CurrentWorldTime = TrackEditorThumbnail.GetEvalPosition();

		// Draw the frame. If our total frame count is < 3 we re-render some benign frames to ensure the view is correctly set up (first few frames can be black)
		do
		{
			InternalViewportScene->Draw(false);
		}
		while (++FrameCount < 3);

		// Ask the client to finalize the frame
		ViewportThumbnailClient->PostDraw(TrackEditorThumbnail, *InternalViewportClient, *InternalViewportScene);

		// Copy the render target into our texture
		TrackEditorThumbnail.CopyTextureIn(InternalViewportScene);
	}
}


void FTrackEditorThumbnailCache::Revalidate(double InCurrentTime)
{
	if (CurrentCache == PreviousCache && !bForceRedraw && !bNeedsNewThumbnails)
	{
		return;
	}

	if (FMath::IsNearlyZero(CurrentCache.TimeRange.Size<float>()) || CurrentCache.TimeRange.IsEmpty())
	{
		// Can't generate thumbnails for this
		ThumbnailPool.Pin()->RemoveThumbnailsNeedingRedraw(Thumbnails);
		ThumbnailsNeedingRedraw.Reset();
		Thumbnails.Reset();
		bNeedsNewThumbnails = false;
		return;
	}

	if (CurrentCache.Quality != PreviousCache.Quality)
	{
		SetupViewportEngineFlags();
	}

	bNeedsNewThumbnails = true;

	if (ShouldRegenerateEverything())
	{
		ThumbnailPool.Pin()->RemoveThumbnailsNeedingRedraw(Thumbnails);
		Thumbnails.Reset();
	}
	
	if (InCurrentTime - LastComputationTime > 0.25f)
	{
		ComputeNewThumbnails();
		LastComputationTime = InCurrentTime;
	}
}

void FTrackEditorThumbnailCache::ComputeNewThumbnails()
{
	ThumbnailsNeedingRedraw.Reset();

	if (CurrentCache.SingleReferenceFrame.IsSet())
	{
		if (!Thumbnails.Num() || bForceRedraw || CurrentCache.SingleReferenceFrame != PreviousCache.SingleReferenceFrame)
		{
			UpdateSingleThumbnail();
		}
	}
	else
	{
		UpdateFilledThumbnails();
	}

	if (ThumbnailsNeedingRedraw.Num())
	{
		ThumbnailPool.Pin()->AddThumbnailsNeedingRedraw(ThumbnailsNeedingRedraw);
	}
	if (Thumbnails.Num())
	{
		Setup();
	}

	bForceRedraw = false;
	bNeedsNewThumbnails = false;
}


void FTrackEditorThumbnailCache::UpdateSingleThumbnail()
{
	Thumbnails.Reset();

	const float TimePerPx = CurrentCache.TimeRange.Size<float>() / CurrentCache.AllottedSize.X;
	const float HalfRange = CurrentCache.DesiredSize.X*TimePerPx*.5f;
	const float EvalPosition = CurrentCache.SingleReferenceFrame.GetValue();

	TSharedPtr<FTrackEditorThumbnail> NewThumbnail = MakeShareable(new FTrackEditorThumbnail(
		FOnThumbnailDraw::CreateRaw(this, &FTrackEditorThumbnailCache::DrawViewportThumbnail),
		CurrentCache.DesiredSize,
		TRange<float>(EvalPosition - HalfRange, EvalPosition + HalfRange),
		EvalPosition
	));

	Thumbnails.Add(NewThumbnail);
	ThumbnailsNeedingRedraw.Add(NewThumbnail);
}


void FTrackEditorThumbnailCache::UpdateFilledThumbnails()
{
	// Remove any thumbnails from the front of the array that aren't in the actual time range of this section (we keep stuff around outside of the visible range)
	{
		int32 Index = 0;
		for (; Index < Thumbnails.Num(); ++Index)
		{
			if (Thumbnails[Index]->GetTimeRange().Overlaps(CurrentCache.TimeRange))
			{
				break;
			}
		}
		if (Index)
		{
			TArray<TSharedPtr<FTrackEditorThumbnail>> Remove;
			Remove.Append(&Thumbnails[0], Index);
			ThumbnailPool.Pin()->RemoveThumbnailsNeedingRedraw(Remove);

			Thumbnails.RemoveAt(0, Index, false);
		}
	}

	// Remove any thumbnails from the back of the array that aren't in the *actual* time range of this section (we keep stuff around outside of the visible range)
	{
		int32 NumToRemove = 0;
		for (int32 Index = Thumbnails.Num() - 1; Index >= 0; --Index)
		{
			if (!Thumbnails[Index]->GetTimeRange().Overlaps(CurrentCache.TimeRange))
			{
				++NumToRemove;
			}
			else
			{
				break;
			}
		}
		
		if (NumToRemove)
		{
			TArray<TSharedPtr<FTrackEditorThumbnail>> Remove;
			Remove.Append(&Thumbnails[Thumbnails.Num() - NumToRemove], NumToRemove);
			ThumbnailPool.Pin()->RemoveThumbnailsNeedingRedraw(Remove);

			Thumbnails.RemoveAt(Thumbnails.Num() - NumToRemove, NumToRemove, false);
		}
	}

	TRange<float> MaxRange(CurrentCache.VisibleRange.GetLowerBoundValue() - CurrentCache.VisibleRange.Size<float>(), CurrentCache.VisibleRange.GetUpperBoundValue() + CurrentCache.VisibleRange.Size<float>());
	TRange<float> Boundary = TRange<float>::Intersection(CurrentCache.TimeRange, MaxRange);

	if (!Boundary.IsEmpty())
	{
		GenerateFront(Boundary);
		GenerateBack(Boundary);
	}

	if (Thumbnails.Num())
	{
		for (const TSharedPtr<FTrackEditorThumbnail>& Thumbnail : Thumbnails)
		{
			Thumbnail->SortOrder = Thumbnail->GetTimeRange().Overlaps(CurrentCache.VisibleRange) ? 1 : 10;
		}
	}
}


void FTrackEditorThumbnailCache::GenerateFront(const TRange<float>& Boundary)
{
	if (!Thumbnails.Num())
	{
		return;
	}

	const float TimePerPx = CurrentCache.TimeRange.Size<float>() / CurrentCache.AllottedSize.X;
	float EndTime = Thumbnails[0]->GetTimeRange().GetLowerBoundValue();

	while (EndTime > Boundary.GetLowerBoundValue())
	{
		FIntPoint TextureSize = CurrentCache.DesiredSize;

		TRange<float> TimeRange(EndTime - TextureSize.X * TimePerPx, EndTime);

		// Evaluate the thumbnail along the length of its duration, based on its position in the sequence
		const float FrameLength = TimeRange.Size<float>();
		float TotalLerp = (TimeRange.GetLowerBoundValue() - CurrentCache.TimeRange.GetLowerBoundValue()) / (CurrentCache.TimeRange.Size<float>() - FrameLength);
		float EvalPosition = CurrentCache.TimeRange.GetLowerBoundValue() + FMath::Clamp(TotalLerp, 0.f, .99f)*CurrentCache.TimeRange.Size<float>();

		TSharedPtr<FTrackEditorThumbnail> NewThumbnail = MakeShareable(new FTrackEditorThumbnail(
			FOnThumbnailDraw::CreateRaw(this, &FTrackEditorThumbnailCache::DrawViewportThumbnail),
			TextureSize,
			TimeRange,
			EvalPosition
		));

		Thumbnails.Insert(NewThumbnail, 0);
		ThumbnailsNeedingRedraw.Add(NewThumbnail);

		EndTime = TimeRange.GetLowerBoundValue();
	}
}


void FTrackEditorThumbnailCache::GenerateBack(const TRange<float>& Boundary)
{
	const float TimePerPx = CurrentCache.TimeRange.Size<float>() / CurrentCache.AllottedSize.X;
	float StartTime = Thumbnails.Num() ? Thumbnails.Last()->GetTimeRange().GetUpperBoundValue() : Boundary.GetLowerBoundValue();

	while (StartTime < Boundary.GetUpperBoundValue())
	{
		FIntPoint TextureSize = CurrentCache.DesiredSize;

		{
			// Move the thumbnail to the center of the space if we're the only thumbnail, and we don't fit on
			float Overflow = TextureSize.X*TimePerPx - CurrentCache.TimeRange.Size<float>();
			if (Thumbnails.Num() == 0 && Overflow > 0)
			{
				StartTime -= Overflow*.5f;
			}
		}

		TRange<float> TimeRange(StartTime, StartTime + TextureSize.X * TimePerPx);

		const float FrameLength = TimeRange.Size<float>();
		float TotalLerp = (TimeRange.GetLowerBoundValue() - CurrentCache.TimeRange.GetLowerBoundValue()) / (CurrentCache.TimeRange.Size<float>() - FrameLength);
		float EvalPosition = CurrentCache.TimeRange.GetLowerBoundValue() + FMath::Clamp(TotalLerp, 0.f, .99f)*CurrentCache.TimeRange.Size<float>();

		TSharedPtr<FTrackEditorThumbnail> NewThumbnail = MakeShareable(new FTrackEditorThumbnail(
			FOnThumbnailDraw::CreateRaw(this, &FTrackEditorThumbnailCache::DrawViewportThumbnail),
			TextureSize,
			TimeRange,
			EvalPosition
		));

		NewThumbnail->SortOrder = TimeRange.Overlaps(CurrentCache.VisibleRange) ? 1 : 10;

		Thumbnails.Add(NewThumbnail);
		ThumbnailsNeedingRedraw.Add(NewThumbnail);

		StartTime = TimeRange.GetUpperBoundValue();
	}
}


void FTrackEditorThumbnailCache::Setup()
{
	// Set up the necessary viewport gubbins for viewport thumbnails
	if (ViewportThumbnailClient)
	{
		if (!InternalViewportClient.IsValid())
		{
			InternalViewportClient = MakeShareable(new FThumbnailViewportClient());
			InternalViewportClient->ViewportType = LVT_Perspective;
			InternalViewportClient->bDisableInput = true;
			InternalViewportClient->bDrawAxes = false;
			InternalViewportClient->SetAllowCinematicPreview(false);
			InternalViewportClient->SetRealtime(false);

			SetupViewportEngineFlags();

			InternalViewportClient->ViewState.GetReference()->SetSequencerState(true);
		}

		if (!InternalViewportScene.IsValid())
		{
			InternalViewportScene = MakeShareable(new FSceneViewport(InternalViewportClient.Get(), nullptr));
			InternalViewportClient->Viewport = InternalViewportScene.Get();
		}
	}
	else if (CustomThumbnailClient)
	{
		CustomThumbnailClient->Setup();
	}
}

void FTrackEditorThumbnailCache::SetupViewportEngineFlags()
{
	if (!ViewportThumbnailClient || !InternalViewportClient.IsValid())
	{
		return;
	}

	InternalViewportClient->EngineShowFlags = FEngineShowFlags(ESFIM_Game);

	switch (CurrentCache.Quality)
	{
	case EThumbnailQuality::Draft:
		InternalViewportClient->EngineShowFlags.DisableAdvancedFeatures();
		break;

	case EThumbnailQuality::Normal:
	case EThumbnailQuality::Best:
		InternalViewportClient->EngineShowFlags.SetMotionBlur(false);
		break;
	}

	InternalViewportClient->Invalidate();
}
