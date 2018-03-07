// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneImagePlateSection.h"

#define LOCTEXT_NAMESPACE "MovieSceneImagePlateSection"

UMovieSceneImagePlateSection::UMovieSceneImagePlateSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	ThumbnailReferenceOffset = 0.f;
#endif

	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
	bReuseExistingTexture = false;

	// Video tracks have some preroll by default to precache frames
	SetPreRollTime(0.5f);
}

#undef LOCTEXT_NAMESPACE