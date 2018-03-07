// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "TrackEditorThumbnail/TrackEditorThumbnail.h"

class FLevelEditorViewportClient;
class FMenuBuilder;
class FSceneViewport;
class FSequencerSectionPainter;
class FTrackEditorThumbnailPool;
class ISectionLayoutBuilder;
struct FSlateBrush;

struct FThumbnailCameraSettings
{
	float AspectRatio;
};


/**
 * Thumbnail section, which paints and ticks the appropriate section.
 */
class MOVIESCENETOOLS_API FThumbnailSection
	: public ISequencerSection
	, public TSharedFromThis<FThumbnailSection>
{
public:

	/** Create and initialize a new instance. */
	FThumbnailSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, IViewportThumbnailClient* InViewportThumbanilClient, UMovieSceneSection& InSection);
	FThumbnailSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, ICustomThumbnailClient* InCustomThumbnailClient, UMovieSceneSection& InSection);

	/** Destructor. */
	~FThumbnailSection();

public:

	/** @return The sequencer widget owning the MovieScene section. */
	TSharedRef<SWidget> GetSequencerWidget()
	{
		return SequencerPtr.Pin()->GetSequencerWidget();
	}

	/** Enter rename mode for the section */
	void EnterRename();

	/** Get whether the text is renameable */
	virtual bool CanRename() const { return false; }

	/** Callback for getting the text of the track name text block. */
	virtual FText HandleThumbnailTextBlockText() const { return FText::GetEmpty(); }

	/** Callback for when the text of the track name text block has changed. */
	virtual void HandleThumbnailTextBlockTextCommitted(const FText& NewThumbnailName, ETextCommit::Type CommitType) { }

public:

	/** Set this thumbnail section to draw a single thumbnail at the specified time */
	virtual void SetSingleTime(float GlobalTime) = 0;

public:

	//~ ISequencerSection interface

	virtual void GenerateSectionLayout(ISectionLayoutBuilder& LayoutBuilder) const override { }
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override;
	virtual TSharedRef<SWidget> GenerateSectionWidget() override;
	virtual float GetSectionGripSize() const override;
	virtual float GetSectionHeight() const override;
	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	/** Called to force a redraw of this section's thumbnails */
	void RedrawThumbnails();

	/** Get the range that is currently visible in the section's time space */
	TRange<float> GetVisibleRange() const;

	/** Get the total range that thumbnails are to be generated for in the section's time space */
	TRange<float> GetTotalRange() const;

protected:

	/** The section we are visualizing. */
	UMovieSceneSection* Section;

	/** The parent sequencer we are a part of. */
	TWeakPtr<ISequencer> SequencerPtr;

	/** A list of all thumbnails this section has. */
	FTrackEditorThumbnailCache ThumbnailCache;

	/** Saved playback status. Used for restoring state when rendering thumbnails */
	EMovieScenePlayerStatus::Type SavedPlaybackStatus;
	
	/** Rename widget */
	TSharedPtr<SInlineEditableTextBlock> NameWidget;

	/** Fade brush. */
	const FSlateBrush* WhiteBrush;

	/** Additional draw effects */
	ESlateDrawEffect AdditionalDrawEffect;

	enum class ETimeSpace
	{
		Global,
		Local,
	};

	/** Enumeration value specifyin in which time-space to generate thumbnails */
	ETimeSpace TimeSpace;

	FDelegateHandle RedrawThumbnailDelegateHandle;
};

/**
 * Thumbnail section, which paints and ticks the appropriate section.
 */
class MOVIESCENETOOLS_API FViewportThumbnailSection
	: public FThumbnailSection
	, public IViewportThumbnailClient
{
public:

	/** Create and initialize a new instance. */
	FViewportThumbnailSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection);

	//~ IViewportThumbnailClient interface
	virtual void PreDraw(FTrackEditorThumbnail& Thumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport) override;
	virtual void PostDraw(FTrackEditorThumbnail& Thumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport) override;
};
