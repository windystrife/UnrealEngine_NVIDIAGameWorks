// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBindingTrack.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "Casts.h"
#include "ControlRigBindingTemplate.h"

#define LOCTEXT_NAMESPACE "ControlRigBindingTrack"

FMovieSceneEvalTemplatePtr UControlRigBindingTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneSpawnSection* Section = CastChecked<const UMovieSceneSpawnSection>(&InSection);
	return FControlRigBindingTemplate(*Section);
}

#if WITH_EDITORONLY_DATA

FText UControlRigBindingTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Bound");
}

#endif

#undef LOCTEXT_NAMESPACE
