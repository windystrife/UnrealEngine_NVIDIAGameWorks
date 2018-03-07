// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneBoolTrack.generated.h"

/**
 * Handles manipulation of float properties in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneBoolTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

	DEPRECATED(4.15, "Direct evaluation of boolean tracks is no longer supported. Please create an evaluation template (see FMovieSceneBoolPropertySectionTemplate).")
	virtual bool Eval( float Position, float LastPostion, bool& InOutBool ) const;
};
