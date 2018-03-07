// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"

class FSlateWindowElementList;
class UMovieSceneSection;
class UMovieSceneTrack;
struct FTimeToPixel;

/** Class that wraps up common section painting functionality */
class SEQUENCER_API FSequencerSectionPainter
{
public:
	/** Constructor */
	FSequencerSectionPainter(FSlateWindowElementList& OutDrawElements, const FGeometry& InSectionGeometry, UMovieSceneSection& Section);

	/** Virtual destructor */
	virtual ~FSequencerSectionPainter();

	/** Paint the section background with the specified tint override */
	virtual int32 PaintSectionBackground(const FLinearColor& Tint) = 0;

	/** Paint the section background with the tint stored on the track */
	int32 PaintSectionBackground();

	/** Get the track that this painter is painting sections for */
	UMovieSceneTrack* GetTrack() const;

	/** Blend the specified color with the default track color */
	static FLinearColor BlendColor(FLinearColor InColor);

public:

	/** Get a time-to-pixel converter for the section */
	virtual const FTimeToPixel& GetTimeConverter() const = 0;

public:

	/** The movie scene section we're painting */
	UMovieSceneSection& Section;

	/** List of slate draw elements - publicly modifiable */
	FSlateWindowElementList& DrawElements;

	/** The full geometry of the section. This is the width of the track area in the case of infinite sections */
	FGeometry SectionGeometry;
	
	/** The full clipping rectangle for the section */
	FSlateRect SectionClippingRect;
	
	/** The layer ID we're painting on */
	int32 LayerId;

	/** Whether our parent widget is enabled or not */
	bool bParentEnabled;

	/** Whether the section is hovered or not */
	bool bIsHighlighted;
};

