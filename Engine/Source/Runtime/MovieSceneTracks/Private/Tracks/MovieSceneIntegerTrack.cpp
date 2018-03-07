// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneIntegerTrack.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

UMovieSceneIntegerTrack::UMovieSceneIntegerTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}


UMovieSceneSection* UMovieSceneIntegerTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneIntegerSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneIntegerTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneIntegerPropertySectionTemplate(*CastChecked<UMovieSceneIntegerSection>(&InSection), *this);
}
