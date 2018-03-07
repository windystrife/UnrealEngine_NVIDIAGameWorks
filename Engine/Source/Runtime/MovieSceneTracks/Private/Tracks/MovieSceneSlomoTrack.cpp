// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneSlomoTrack.h"
#include "Sections/MovieSceneSlomoSection.h"
#include "Evaluation/MovieSceneSlomoTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Templates/Casts.h"

#define LOCTEXT_NAMESPACE "MovieSceneSlomoTrack"


/* UMovieSceneEventTrack interface
 *****************************************************************************/
UMovieSceneSlomoTrack::UMovieSceneSlomoTrack(const FObjectInitializer& Init)
	: Super(Init)
{
	EvalOptions.bCanEvaluateNearestSection = true;
}

UMovieSceneSection* UMovieSceneSlomoTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneSlomoSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneSlomoTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneSlomoSectionTemplate(*CastChecked<UMovieSceneSlomoSection>(&InSection));
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneSlomoTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Play Rate");
}

#endif


#undef LOCTEXT_NAMESPACE
