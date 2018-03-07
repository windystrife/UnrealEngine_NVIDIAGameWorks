// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSubTrack.h"
#include "MovieSceneControlRigTrack.generated.h"

class UControlRigSequence;

/**
 * Handles animation of skeletal mesh actors using animation ControlRigs
 */
UCLASS(MinimalAPI)
class UMovieSceneControlRigTrack
	: public UMovieSceneSubTrack
{
	GENERATED_UCLASS_BODY()

public:

	/** Adds a new animation to this track */
	virtual void AddNewControlRig(float KeyTime, UControlRigSequence* InSequence);

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif
};
