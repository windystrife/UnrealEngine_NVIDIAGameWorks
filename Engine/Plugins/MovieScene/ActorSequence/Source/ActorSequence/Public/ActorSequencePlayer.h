// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePlayer.h"
#include "ActorSequence.h"
#include "MovieSceneSequencePlayer.h"
#include "ActorSequencePlayer.generated.h"

/**
 * UActorSequencePlayer is used to actually "play" an actor sequence asset at runtime.
 */
UCLASS(BlueprintType)
class ACTORSEQUENCE_API UActorSequencePlayer
	: public UMovieSceneSequencePlayer
{
public:
	GENERATED_BODY()

protected:

	//~ IMovieScenePlayer interface
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;
};

