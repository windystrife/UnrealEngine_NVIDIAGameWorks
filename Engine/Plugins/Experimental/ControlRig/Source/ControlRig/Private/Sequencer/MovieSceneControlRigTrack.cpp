// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneControlRigTrack.h"
#include "MovieSceneControlRigSection.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "ControlRigSequence.h"
#include "MovieScene.h"
#include "ControlRigBindingTrack.h"

#define LOCTEXT_NAMESPACE "MovieSceneControlRigTrack"

UMovieSceneControlRigTrack::UMovieSceneControlRigTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(108, 53, 0, 65);
#endif

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}

void UMovieSceneControlRigTrack::AddNewControlRig(float KeyTime, UControlRigSequence* InSequence)
{
	UMovieSceneControlRigSection* NewSection = Cast<UMovieSceneControlRigSection>(CreateNewSection());
	{
		NewSection->InitialPlacement(Sections, KeyTime, KeyTime + InSequence->GetMovieScene()->GetPlaybackRange().Size<float>(), SupportsMultipleRows());
		NewSection->SetSequence(InSequence);
	}

	AddSection(*NewSection);
}

UMovieSceneSection* UMovieSceneControlRigTrack::CreateNewSection()
{
	return NewObject<UMovieSceneControlRigSection>(this);
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneControlRigTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "ControlRig");
}

#endif

#undef LOCTEXT_NAMESPACE
