// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneMarginTrack.generated.h"

/**
 * Handles manipulation of FMargins in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneMarginTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	UMovieSceneMarginTrack(const FObjectInitializer& Init);
	
	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
};
