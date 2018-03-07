// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerSectionPainter.h"
#include "MovieSceneTrack.h"

FSequencerSectionPainter::FSequencerSectionPainter(FSlateWindowElementList& OutDrawElements, const FGeometry& InSectionGeometry, UMovieSceneSection& InSection)
	: Section(InSection)
	, DrawElements(OutDrawElements)
	, SectionGeometry(InSectionGeometry)
	, LayerId(0)
	, bParentEnabled(true)
	, bIsHighlighted(false)
{
}

FSequencerSectionPainter::~FSequencerSectionPainter()
{
}

int32 FSequencerSectionPainter::PaintSectionBackground()
{
	return PaintSectionBackground(GetTrack()->GetColorTint());
}

UMovieSceneTrack* FSequencerSectionPainter::GetTrack() const
{
	return Section.GetTypedOuter<UMovieSceneTrack>();
}

FLinearColor FSequencerSectionPainter::BlendColor(FLinearColor InColor)
{
	static FLinearColor BaseColor(FColor(71,71,71));

	const float Alpha = InColor.A;
	InColor.A = 1.f;
	
	return BaseColor * (1.f - Alpha) + InColor * Alpha;
}
