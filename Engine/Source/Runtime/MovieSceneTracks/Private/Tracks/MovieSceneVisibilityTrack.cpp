// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Evaluation/MovieSceneVisibilityTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneVisibilityTrack"


UMovieSceneVisibilityTrack::UMovieSceneVisibilityTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}


FMovieSceneEvalTemplatePtr UMovieSceneVisibilityTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneVisibilitySectionTemplate(*CastChecked<const UMovieSceneBoolSection>(&InSection), *this);
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneVisibilityTrack::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Visibility");
}

#endif


#undef LOCTEXT_NAMESPACE
