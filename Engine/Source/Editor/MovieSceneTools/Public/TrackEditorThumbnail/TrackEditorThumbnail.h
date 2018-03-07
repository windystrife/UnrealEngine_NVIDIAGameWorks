// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "HAL/ThreadSafeBool.h"
#include "Animation/CurveSequence.h"
#include "Rendering/RenderingCommon.h"
#include "MovieSceneToolsUserSettings.h"
#include "RHI.h"

class FLevelEditorViewportClient;
class FSceneViewport;
class FSlateShaderResource;
class FSlateTexture2DRHIRef;
class FThumbnailViewportClient;
class FTrackEditorThumbnail;
class FTrackEditorThumbnailPool;

DECLARE_DELEGATE_OneParam(FOnThumbnailDraw, FTrackEditorThumbnail&);

/**
 * Track Editor Thumbnail, which keeps a Texture to be displayed by a viewport.
 */
class MOVIESCENETOOLS_API FTrackEditorThumbnail
	: public ISlateViewport
	, public TSharedFromThis<FTrackEditorThumbnail>
{
public:

	/** Create and initialize a new instance. */
	FTrackEditorThumbnail(const FOnThumbnailDraw& InOnDraw, const FIntPoint& InSize, TRange<float> InTimeRange, float InPosition);

	/** Virtual destructor. */
	virtual ~FTrackEditorThumbnail();

public:

	/** Copies the incoming render target to this thumbnails texture. */
	void CopyTextureIn(TSharedPtr<FSceneViewport> SceneViewport);
	
	/** Copies the specified texture to this thumbnail's texture while maintining the correct ratios */
	void CopyTextureIn(FTexture2DRHIRef SourceTexture);

	/** Renders the thumbnail to the texture. */
	void DrawThumbnail();

	/** Prompt this thumbnail to fade in */
	void SetupFade(const TSharedRef<SWidget>& InWidget);
	void PlayFade();

	/** Gets the curve for fading in the thumbnail. */
	float GetFadeInCurve() const;

	/** Get the full time-range that this thumbnail occupies */
	const TRange<float> GetTimeRange() const { return TimeRange; }

	/** Get the time at which this thumbnail should be drawn */
	float GetEvalPosition() const { return Position; }

public:

	// ISlateViewport interface

	virtual FIntPoint GetSize() const override;
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual bool RequiresVsync() const override;

private:

	/** Destroy the texture */
	void DestroyTexture();

public:

	/** Sort order for this thumbnail */
	int32 SortOrder;

	/** True when this thumbnail has been drawn, false otherwise */
	FThreadSafeBool bHasFinishedDrawing;

private:

	/** Delegate to use to draw the thumbnail. */
	FOnThumbnailDraw OnDraw;

	/** The size of the texture */
	FIntPoint Size;

	/** The Texture RHI that holds the thumbnail. */
	FSlateTexture2DRHIRef* Texture;

	/** Where in time this thumbnail is a rendering of. */
	TRange<float> TimeRange;

	/** The position we should actually render (within the above time range). */
	float Position;

	/** Fade curve to display while the thumbnail is redrawing. */
	FCurveSequence FadeInCurve;

	/** Strong reference to a scene viewport we're currently copying from */
	TSharedPtr<FSceneViewport> SceneViewportReference;
};

/** Client interface for thumbanils that render the current world from a viewport */
struct IViewportThumbnailClient
{
	virtual void PreDraw(FTrackEditorThumbnail& TrackEditorThumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport) { }
	virtual void PostDraw(FTrackEditorThumbnail& TrackEditorThumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport) { }
};

/** Custom thumbnail drawing client interface */
struct ICustomThumbnailClient
{
	virtual void Setup() {}
	virtual void Draw(FTrackEditorThumbnail& TrackEditorThumbnail) { }
};

/** Cache data */
struct FThumbnailCacheData
{
	FThumbnailCacheData() : VisibleRange(0.f), TimeRange(0.f), AllottedSize(0,0), DesiredSize(0, 0), Quality(EThumbnailQuality::Normal) {}

	bool operator==(const FThumbnailCacheData& RHS) const
	{
		return
			AllottedSize == RHS.AllottedSize && 
			VisibleRange == RHS.VisibleRange &&
			TimeRange == RHS.TimeRange &&
			DesiredSize == RHS.DesiredSize &&
			Quality == RHS.Quality &&
			SingleReferenceFrame == RHS.SingleReferenceFrame;
	}

	bool operator!=(const FThumbnailCacheData& RHS) const
	{
		return
			AllottedSize != RHS.AllottedSize ||
			VisibleRange != RHS.VisibleRange ||
			TimeRange != RHS.TimeRange ||
			DesiredSize != RHS.DesiredSize ||
			Quality != RHS.Quality ||
			SingleReferenceFrame != RHS.SingleReferenceFrame;
	}

	/** The visible range of our thumbnails we can see on the UI */
	TRange<float> VisibleRange;
	/** The total range to generate thumbnails for */
	TRange<float> TimeRange;
	/** Physical size of the thumbnail area */
	FIntPoint AllottedSize;
	/** Desired frame size constraint */
	FIntPoint DesiredSize;
	/** Thumbnail quality */
	EThumbnailQuality Quality;
	/** Set when we want to render a single reference frame */
	TOptional<float> SingleReferenceFrame;
};

class MOVIESCENETOOLS_API FTrackEditorThumbnailCache
{
public:
	FTrackEditorThumbnailCache(const TSharedPtr<FTrackEditorThumbnailPool>& ThumbnailPool, IViewportThumbnailClient* InViewportThumbnailClient);
	FTrackEditorThumbnailCache(const TSharedPtr<FTrackEditorThumbnailPool>& ThumbnailPool, ICustomThumbnailClient* InCustomThumbnailClient);
	
	~FTrackEditorThumbnailCache();

	void ForceRedraw() { bForceRedraw = true; }

	void SetSingleReferenceFrame(TOptional<float> InReferenceFrame);
	TOptional<float> GetSingleReferenceFrame() const { return CurrentCache.SingleReferenceFrame; }

	void Update(const TRange<float>& NewRange, const TRange<float>& VisibleRange, const FIntPoint& AllottedSize, const FIntPoint& InDesiredSize, EThumbnailQuality InQuality, double InCurrentTime);

	void Revalidate(double InCurrentTime);

	void DrawViewportThumbnail(FTrackEditorThumbnail& TrackEditorThumbnail);

	const TArray<TSharedPtr<FTrackEditorThumbnail>>& GetThumbnails() const
	{
		return Thumbnails;
	}

protected:

	void ComputeNewThumbnails();

	void Setup();

	bool ShouldRegenerateEverything() const;

	FIntPoint CalculateTextureSize() const;

	void UpdateSingleThumbnail();

	void UpdateFilledThumbnails();

	void GenerateFront(const TRange<float>& Boundary);

	void GenerateBack(const TRange<float>& Boundary);

	void SetupViewportEngineFlags();

protected:

	/** Thumbnail client used for paint notifications */
	IViewportThumbnailClient* ViewportThumbnailClient;
	ICustomThumbnailClient* CustomThumbnailClient;

	/** An internal viewport scene we use to render the thumbnails with. */
	TSharedPtr<FSceneViewport> InternalViewportScene;

	/** An internal editor viewport client to render the thumbnails with. */
	TSharedPtr<class FThumbnailViewportClient> InternalViewportClient;

	/** The thumbnail pool that we are sending all of our thumbnails to. */
	TWeakPtr<FTrackEditorThumbnailPool> ThumbnailPool;

	FThumbnailCacheData CurrentCache;

	FThumbnailCacheData PreviousCache;

	TArray<TSharedPtr<FTrackEditorThumbnail>> Thumbnails;
	TArray<TSharedPtr<FTrackEditorThumbnail>> ThumbnailsNeedingRedraw;

	/** The number of frames we've rendered */
	uint32 FrameCount;

	double LastComputationTime;
	bool bNeedsNewThumbnails;

	/** Whether to force a redraw or not */
	bool bForceRedraw;
};
