// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneActorReferenceTrack.h"
#include "Sections/MovieSceneActorReferenceSection.h"
#include "Evaluation/MovieSceneActorReferenceTemplate.h"


UMovieSceneActorReferenceTrack::UMovieSceneActorReferenceTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneActorReferenceTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneActorReferenceSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneActorReferenceTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneActorReferenceSectionTemplate(*CastChecked<UMovieSceneActorReferenceSection>(&InSection), *this);
}

