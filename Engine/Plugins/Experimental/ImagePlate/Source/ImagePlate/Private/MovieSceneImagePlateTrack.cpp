// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneImagePlateTrack.h"
#include "MovieSceneImagePlateSection.h"
#include "MovieSceneImagePlateTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneImagePlateTrack"

UMovieSceneImagePlateTrack::UMovieSceneImagePlateTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(0, 0, 0, 200);
#endif

	// By default, evaluate ImagePlates in pre and postroll
	EvalOptions.bEvaluateInPreroll = EvalOptions.bEvaluateInPostroll = true;
}

void UMovieSceneImagePlateTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.AddUnique(&Section);
}

UMovieSceneSection* UMovieSceneImagePlateTrack::CreateNewSection()
{
	return NewObject<UMovieSceneImagePlateSection>(this, NAME_None, RF_Transactional);
}

const TArray<UMovieSceneSection*>& UMovieSceneImagePlateTrack::GetAllSections() const
{
	return Sections;
}

void UMovieSceneImagePlateTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

FMovieSceneEvalTemplatePtr UMovieSceneImagePlateTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneImagePlateSectionTemplate(*CastChecked<const UMovieSceneImagePlateSection>(&InSection), *this);
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneImagePlateTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("DefaultDisplayName", "Image Plate Track");
}
#endif

#undef LOCTEXT_NAMESPACE