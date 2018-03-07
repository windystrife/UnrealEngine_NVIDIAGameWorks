// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaTrack.h"

#include "MovieSceneMediaSection.h"
#include "MovieSceneMediaTemplate.h"


#define LOCTEXT_NAMESPACE "MovieSceneMediaTrack"


/* UMovieSceneMediaTrack interface
 *****************************************************************************/

UMovieSceneMediaTrack::UMovieSceneMediaTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvalOptions.bCanEvaluateNearestSection = false;
	EvalOptions.bEvalNearestSection = false;
	EvalOptions.bEvaluateInPreroll = true;
	EvalOptions.bEvaluateInPostroll = true;

#if WITH_EDITORONLY_DATA
	TrackTint = FColor(0, 0, 0, 200);
#endif
}


/* UMovieScenePropertyTrack interface
 *****************************************************************************/

void UMovieSceneMediaTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.AddUnique(&Section);
}


UMovieSceneSection* UMovieSceneMediaTrack::CreateNewSection()
{
	return NewObject<UMovieSceneMediaSection>(this, NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneMediaTrack::GetAllSections() const
{
	return Sections;
}


void UMovieSceneMediaTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}


FMovieSceneEvalTemplatePtr UMovieSceneMediaTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneMediaSectionTemplate(*CastChecked<const UMovieSceneMediaSection>(&InSection), *this);
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneMediaTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("DefaultDisplayName", "Media Track");
}


FName UMovieSceneMediaTrack::GetTrackName() const
{
	return UniqueTrackName;
}

#endif


#undef LOCTEXT_NAMESPACE
