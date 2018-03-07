// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneFadeSection.h"


/* UMovieSceneFadeSection structors
 *****************************************************************************/

UMovieSceneFadeSection::UMovieSceneFadeSection()
	: UMovieSceneFloatSection()
	, FadeColor(FLinearColor::Black)
	, bFadeAudio(false)
{
	SetIsInfinite(true);

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}
