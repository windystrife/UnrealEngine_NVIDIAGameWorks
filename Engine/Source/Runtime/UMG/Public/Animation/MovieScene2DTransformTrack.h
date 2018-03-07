// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieScene2DTransformTrack.generated.h"

struct FWidgetTransform;

/**
 * Handles manipulation of 2D transforms in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieScene2DTransformTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	UMovieScene2DTransformTrack(const FObjectInitializer& ObjectInitializer);
	
	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	
public:

	DEPRECATED(4.15, "Please evaluate using FMovieScene2DTransformTemplate.")
	bool Eval( float Position, float LastPostion, FWidgetTransform& InOutTransform ) const;
};
