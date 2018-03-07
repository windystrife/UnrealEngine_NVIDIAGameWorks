// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneColorTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneColorSection.h"
#include "Evaluation/MovieSceneColorTemplate.h"

UMovieSceneColorTrack::UMovieSceneColorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}


UMovieSceneSection* UMovieSceneColorTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneColorSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneColorTrack::CreateTemplateForSection(const UMovieSceneSection& Section) const
{
	return FMovieSceneColorSectionTemplate(*CastChecked<const UMovieSceneColorSection>(&Section), *this);
}
