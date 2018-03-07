// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FTrackEditorThumbnail;
class ISequencer;

/**
 * Track Editor Thumbnail pool, which keeps a list of thumbnails that
 * need to be drawn and draws them incrementally.
 */
class MOVIESCENETOOLS_API FTrackEditorThumbnailPool
{
public:

	FTrackEditorThumbnailPool(TSharedPtr<ISequencer> InSequencer);

public:

	/** Requests that the passed in thumbnails need to be drawn */
	void AddThumbnailsNeedingRedraw(const TArray<TSharedPtr<FTrackEditorThumbnail>>& InThumbnails);

	/** Draws a small number of thumbnails that are enqueued for drawing */
	/* @return Whether thumbnails were drawn */
	bool DrawThumbnails();

	/** Informs the pool that the thumbnails passed in no longer need to be drawn */
	void RemoveThumbnailsNeedingRedraw(const TArray< TSharedPtr<FTrackEditorThumbnail>>& InThumbnails);

private:

	/** Parent sequencer we're drawing thumbnails for */
	TWeakPtr<ISequencer> Sequencer;

	/** Thumbnails enqueued for drawing */
	TArray< TSharedPtr<FTrackEditorThumbnail> > ThumbnailsNeedingDraw;

	/** Thumbnails that are currently being drawn */
	TArray< TSharedPtr<FTrackEditorThumbnail> > ThumbnailsBeingDrawn;

	double TimeOfLastDraw;
	double TimeOfLastUpdate;

	/** Whether we need to sort */
	bool bNeedsSort;
};
