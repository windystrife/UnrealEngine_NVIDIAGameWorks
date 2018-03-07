// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneCameraAnimSection.h"
#include "Evaluation/MovieSceneCameraAnimTemplate.h"


UMovieSceneCameraAnimSection::UMovieSceneCameraAnimSection(const FObjectInitializer& ObjectInitializer)
 	: Super( ObjectInitializer )
{
	CameraAnim_DEPRECATED = nullptr;
	PlayRate_DEPRECATED = 1.f;
	PlayScale_DEPRECATED = 1.f;
	BlendInTime_DEPRECATED = 0.f;
	BlendOutTime_DEPRECATED = 0.f;
	bLooping_DEPRECATED = false;

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}

void UMovieSceneCameraAnimSection::PostLoad()
{
	if (CameraAnim_DEPRECATED != nullptr)
	{
		AnimData.CameraAnim = CameraAnim_DEPRECATED;
	}

	if (PlayRate_DEPRECATED != 1.f)
	{
		AnimData.PlayRate = PlayRate_DEPRECATED;
	}

	if (PlayScale_DEPRECATED != 1.f)
	{
		AnimData.PlayScale = PlayScale_DEPRECATED;
	}

	if (BlendInTime_DEPRECATED != 0.f)
	{
		AnimData.BlendInTime = BlendInTime_DEPRECATED;
	}

	if (BlendOutTime_DEPRECATED != 0.f)
	{
		AnimData.BlendOutTime = BlendOutTime_DEPRECATED;
	}

	if (bLooping_DEPRECATED)
	{
		AnimData.bLooping = bLooping_DEPRECATED;
	}

	Super::PostLoad();
}

FMovieSceneEvalTemplatePtr UMovieSceneCameraAnimSection::GenerateTemplate() const
{
	if (AnimData.CameraAnim)
	{
		return FMovieSceneCameraAnimSectionTemplate(*this);
	}
	return FMovieSceneEvalTemplatePtr();
}
