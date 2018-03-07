// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "MovieSceneVisibilityTrack.generated.h"

/**
 * Handles manipulation of visibility properties in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieSceneVisibilityTrack
	: public UMovieSceneBoolTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif
};
