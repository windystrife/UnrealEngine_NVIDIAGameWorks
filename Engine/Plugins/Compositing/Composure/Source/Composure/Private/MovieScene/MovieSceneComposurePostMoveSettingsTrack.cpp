// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneComposurePostMoveSettingsTrack.h"
#include "MovieSceneComposurePostMoveSettingsSection.h"
#include "MovieSceneComposurePostMoveSettingsSectionTemplate.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

UMovieSceneComposurePostMoveSettingsTrack::UMovieSceneComposurePostMoveSettingsTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(48, 227, 255, 65);
#endif
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}

UMovieSceneSection* UMovieSceneComposurePostMoveSettingsTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneComposurePostMoveSettingsSection::StaticClass(), NAME_None, RF_Transactional);
}


FMovieSceneEvalTemplatePtr UMovieSceneComposurePostMoveSettingsTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneComposurePostMoveSettingsSectionTemplate(*CastChecked<const UMovieSceneComposurePostMoveSettingsSection>(&InSection), *this);
}
