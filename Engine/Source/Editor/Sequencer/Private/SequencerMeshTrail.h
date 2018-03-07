// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameFramework/Actor.h"
#include "SequencerMeshTrail.generated.h"

/** Stores time and actor reference for each key Actor on the trail */
struct FKeyActorData
{
	float Time;
	class ASequencerKeyActor* KeyActor;

	FKeyActorData(float InTime, class ASequencerKeyActor* InKeyActor) :
		Time(InTime),
		KeyActor(InKeyActor)
	{
	}
};

/** Stores time and component reference for each frame Component on the trail */
struct FFrameComponentData
{
	float Time;
	class UStaticMeshComponent* FrameComponent;

	FFrameComponentData(float InTime, class UStaticMeshComponent* InFrameComponent) :
		Time(InTime),
		FrameComponent(InFrameComponent)
	{
	}
};

UCLASS()
class ASequencerMeshTrail : public AActor
{
	GENERATED_BODY()

public:
	ASequencerMeshTrail();

	/** Clean up the key mesh Actors and then destroy the trail itself */
	void Cleanup();

	/** Add a SequencerKeyMesh Actor associated with the given track section at the KeyTransform. KeyTime is used as a key to the KeyMeshes TMap */
	void AddKeyMeshActor(float KeyTime, const FTransform KeyTransform, class UMovieScene3DTransformSection* TrackSection);

	/** Add a static mesh component at the KeyTransform. KeyTime is used as a key to the FrameMeshes TMap */
	void AddFrameMeshComponent(const float FrameTime, const FTransform FrameTransform);

	virtual bool IsEditorOnly() const final
	{
		return true;
	}

private:

	/** Array of all Key Mesh Actors for a given trail and the key time they represent */
	TArray<FKeyActorData> KeyMeshActors;

	/** Array of all Frame Mesh Components for a given trail and the frame time they represent */
	TArray<FFrameComponentData> FrameMeshComponents;

	TSharedPtr<class FSequencer> Sequencer;

	void UpdateTrailAppearance(float UpdateTime);
	float TrailTime;
	float MaxTrailTime;
	FTimerHandle TrailUpdate; 
};