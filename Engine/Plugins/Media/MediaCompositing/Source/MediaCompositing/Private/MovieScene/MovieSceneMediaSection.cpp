// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaSection.h"


#define LOCTEXT_NAMESPACE "MovieSceneMediaSection"


/* UMovieSceneMediaSection interface
 *****************************************************************************/

UMovieSceneMediaSection::UMovieSceneMediaSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	ThumbnailReferenceOffset = 0.f;
#endif

	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;

	// media tracks have some preroll by default to precache frames
	SetPreRollTime(0.5f);
}


#undef LOCTEXT_NAMESPACE
