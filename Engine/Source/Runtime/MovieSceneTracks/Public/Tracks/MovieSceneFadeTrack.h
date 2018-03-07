// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "MovieSceneFadeTrack.generated.h"

/**
 * Implements a movie scene track that controls a fade.
 */
UCLASS(MinimalAPI)
class UMovieSceneFadeTrack
	: public UMovieSceneFloatTrack
{
	GENERATED_BODY()

public:

	UMovieSceneFadeTrack(const FObjectInitializer& Init);

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
	virtual bool CanRename() const override { return true; }
#endif
};
