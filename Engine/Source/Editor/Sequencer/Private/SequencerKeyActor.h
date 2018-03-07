// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/StaticMeshActor.h"
#include "SequencerKeyActor.generated.h"

UCLASS()
class SEQUENCER_API ASequencerKeyActor : public AActor
{
	GENERATED_BODY()

public:

	ASequencerKeyActor();

	/** AActor overrides */
	virtual void PostEditMove(bool bFinished) override;
	virtual bool IsEditorOnly() const final
	{
		return true;
	}
	/** AActor */

	/** Set the track section and time associated with this key */
	void SetKeyData(class UMovieScene3DTransformSection* NewTrackSection, float NewKeyTime);

	/** Return the actor associated with this key */
	AActor* GetAssociatedActor() const
	{ 
		return AssociatedActor; 
	}

	/** Returns the mesh component for this key, or nullptr if not spawned right now */
	class UStaticMeshComponent* GetMeshComponent()
	{
		return KeyMeshComponent;
	}

protected:

	/** Push the data from a key actor change back to Sequencer */
	void PropagateKeyChange();

private:

	/** The key mesh */
	UPROPERTY()
	class UStaticMeshComponent* KeyMeshComponent;

	/** The actor whose transform was used to build this key */
	UPROPERTY()
	AActor*	AssociatedActor;

	/** The track section this key resides on */
	UPROPERTY()
	class UMovieScene3DTransformSection* TrackSection;

	/** The time this key is associated with in Sequencer */
	UPROPERTY()
	float KeyTime;
};