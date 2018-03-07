// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "MovieSceneSequencePlayer.h"
#include "ActorSequenceComponent.generated.h"


class UActorSequence;
class UActorSequencePlayer;


/**
 * Movie scene animation embedded within an actor.
 */
UCLASS(Blueprintable, Experimental, ClassGroup=Sequence, hidecategories=(Collision, Cooking, Activation), meta=(BlueprintSpawnableComponent))
class ACTORSEQUENCE_API UActorSequenceComponent
	: public UActorComponent
{
public:
	GENERATED_BODY()

	UActorSequenceComponent(const FObjectInitializer& Init);

	UActorSequence* GetSequence() const
	{
		return Sequence;
	}

	UActorSequencePlayer* GetSequencePlayer() const 
	{
		return SequencePlayer;
	}
	
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

protected:

	UPROPERTY(EditAnywhere, Category="Playback", meta=(ShowOnlyInnerProperties))
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	/** Embedded actor sequence data */
	UPROPERTY(EditAnywhere, Instanced, Category=Animation)
	UActorSequence* Sequence;

	UPROPERTY(transient, BlueprintReadOnly, Category=Animation)
	UActorSequencePlayer* SequencePlayer;

	UPROPERTY(EditAnywhere, Category="Playback")
	bool bAutoPlay;
};
