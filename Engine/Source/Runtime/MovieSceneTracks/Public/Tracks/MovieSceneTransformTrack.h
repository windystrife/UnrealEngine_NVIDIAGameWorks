// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneTransformTrack.generated.h"

struct FMovieSceneInterrogationKey;

/**
 * Handles manipulation of 3D transform properties in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieSceneTransformTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

	/**
	 * Access the interrogation key for transform data - any interrgation data stored with this key is guaranteed to be of type 'FTransform'
	 */
	MOVIESCENETRACKS_API static FMovieSceneInterrogationKey GetInterrogationKey();
};

