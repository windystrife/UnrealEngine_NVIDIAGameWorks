// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieScene.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneIntegerTrack.generated.h"

/**
 * Handles manipulation of integer properties in a movie scene
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneIntegerTrack : public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
};
