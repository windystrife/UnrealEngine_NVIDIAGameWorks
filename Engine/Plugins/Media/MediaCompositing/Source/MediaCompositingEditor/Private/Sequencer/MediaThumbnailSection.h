// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sections/ThumbnailSection.h"
#include "Templates/SharedPointer.h"
#include "TrackEditorThumbnail/TrackEditorThumbnail.h"
#include "UObject/GCObject.h"

class FTrackEditorThumbnailPool;
class ISequencer;
class UMediaPlayer;
class UMediaTexture;
class UMovieSceneMediaSection;


/**
 * Implements a thumbnail section for media tracks.
 */
class FMediaThumbnailSection
	: public FGCObject
	, public FThumbnailSection
	, public ICustomThumbnailClient
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InSection The movie scene section associated with this thumbnail section.
	 * @param InThumbnailPool The thumbnail pool to use for drawing media frame thumbnails.
	 * @param InSequencer The Sequencer object that owns this section.
	 */
	FMediaThumbnailSection(UMovieSceneMediaSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FMediaThumbnailSection();

public:

	//~ FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

public:

	//~ FThumbnailSection interface

	virtual FText GetSectionTitle() const override;
	virtual void SetSingleTime(float GlobalTime) override;
	virtual TSharedRef<SWidget> GenerateSectionWidget() override;
	virtual float GetSectionHeight() const override;
	virtual FMargin GetContentPadding() const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;

public:

	//~ ICustomThumbnailClient interface

	virtual void Draw(FTrackEditorThumbnail& TrackEditorThumbnail) override;
	virtual void Setup() override;

private:

	/** Internal media player used to generate the thumbnail images. */
	UMediaPlayer* MediaPlayer;

	/** Media texture that receives the thumbnail image frames. */
	UMediaTexture* MediaTexture;
};
