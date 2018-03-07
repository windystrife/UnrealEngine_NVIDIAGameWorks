// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneCameraShakeSection.h"
#include "Tracks/MovieSceneCameraShakeTrack.h"
#include "Evaluation/MovieSceneCameraAnimTemplate.h"


UMovieSceneCameraShakeSection::UMovieSceneCameraShakeSection(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
{
	ShakeClass_DEPRECATED = nullptr;
	PlayScale_DEPRECATED = 1.f;
	PlaySpace_DEPRECATED = ECameraAnimPlaySpace::CameraLocal;
	UserDefinedPlaySpace_DEPRECATED = FRotator::ZeroRotator;

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}

void UMovieSceneCameraShakeSection::PostLoad()
{
	if (ShakeClass_DEPRECATED != nullptr)
	{
		ShakeData.ShakeClass = ShakeClass_DEPRECATED;
	}

	if (PlayScale_DEPRECATED != 1.f)
	{
		ShakeData.PlayScale = PlayScale_DEPRECATED;
	}

	if (PlaySpace_DEPRECATED != ECameraAnimPlaySpace::CameraLocal)
	{
		ShakeData.PlaySpace = PlaySpace_DEPRECATED;
	}

	if (UserDefinedPlaySpace_DEPRECATED != FRotator::ZeroRotator)
	{
		ShakeData.UserDefinedPlaySpace = UserDefinedPlaySpace_DEPRECATED;
	}

	Super::PostLoad();
}

FMovieSceneEvalTemplatePtr UMovieSceneCameraShakeSection::GenerateTemplate() const
{
	if (*ShakeData.ShakeClass)
	{
		return FMovieSceneCameraShakeSectionTemplate(*this);
	}
	return FMovieSceneEvalTemplatePtr();
}
