// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InterpGroupInst.generated.h"

class AActor;
class UInterpGroup;

UCLASS(MinimalAPI)
class UInterpGroupInst : public UObject
{
	GENERATED_UCLASS_BODY()

	/**
	 *
	 * 
	 * An instance of an UInterpGroup for a particular Actor. There may be multiple InterpGroupInsts for a single
	 * UInterpGroup in the InterpData, if multiple Actors are connected to the same UInterpGroup. 
	 * The Outer of an UInterpGroupInst is a MatineeActor
	 */

	/** UInterpGroup within the InterpData that this is an instance of. */
	UPROPERTY()
	class UInterpGroup* Group;

	/** 
	 *	Actor that this Group instance is acting upon.
	 *	NB: that this may be set to NULL at any time as a result of the  AActor  being destroyed.
	 */
	UPROPERTY()
	class AActor* GroupActor;

	/** Array if InterpTrack instances. TrackInst.Num() == UInterpGroup.InterpTrack.Num() must be true. */
	UPROPERTY()
	TArray<class UInterpTrackInst*> TrackInst;


	/** 
	 *	Return the AActor that this GroupInstance is working on.
	 *	Should use this instead of just referencing GroupActor, as it check IsPendingKill() for you.
	 */
	virtual AActor* GetGroupActor() const;

	/** Sets the GroupActor tha this GroupInstance should work on. */
	void SetGroupActor(AActor* Actor);

	/** Called before Interp editing to save original state of Actor. @see UInterpTrackInst::SaveActorState */
	virtual void SaveGroupActorState();

	/** Called after Interp editing to put object back to its original state. @see UInterpTrackInst::RestoreActorState */
	virtual void RestoreGroupActorState();

	/**  
	 * Return whether this group contains this Actor.
	 */
	virtual bool HasActor(const AActor* InActor) const
	{
		return (GetGroupActor() == InActor);
	};

	/** 
	 *	Initialize this Group instance. Called from AMatineeActor::InitInterp before doing any interpolation.
	 *	Save the  AActor  for the group and creates any needed InterpTrackInsts
	 */
	virtual void InitGroupInst(UInterpGroup* InGroup, AActor* InGroupActor);

	/** 
	 *	Called when done with interpolation sequence. Cleans up InterpTrackInsts etc. 
	 *	Do not do anything further with the Interpolation after this.
	 */
	virtual void TermGroupInst(bool bDeleteTrackInst);
};

