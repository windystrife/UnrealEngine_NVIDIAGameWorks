// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * 
 * The Outer of an InterpTrackInst is the UInterpGroupInst.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InterpTrackInst.generated.h"

class AActor;
class UInterpTrack;

UCLASS(MinimalAPI)
class UInterpTrackInst : public UObject
{
	GENERATED_UCLASS_BODY()


	/** 
	 *	Return the  AActor  associated with this instance of a Group. 
	 *	Note that all Groups have at least 1 instance, even if no  AActor  variable is attached, so this may return NULL. 
	 */
	ENGINE_API AActor* GetGroupActor() const;

	/** 
	 * Save any variables from the actor that will be modified by this instance.
	 *
	 * @param	Track	The track associated to this instance.
	 */
	virtual void SaveActorState(UInterpTrack* Track) {}

	/** 
	 * Restores any variables modified on the actor by this instance.
	 *
	 * @param	Track	The track associated to this instance.
	 */
	virtual void RestoreActorState(UInterpTrack* Track) {}

	/**  
	 * Initialize the track instance. Called in-game before doing any interpolation
	 *
	 * @param	Track	The track associated to this instance.
	 */
	virtual void InitTrackInst(UInterpTrack* Track) {}

	/** 
	* Called when interpolation is done. Should not do anything else with this TrackInst after this.
	*
	* @param	Track	The track associated to this instance.
	*/
	virtual void TermTrackInst(UInterpTrack* Track) {}

	/** 
	* Get the world to which the GroupActor associated with this instance of a Group belongs
	*
	* returns the GroupActor's world
	*/
	ENGINE_API virtual UWorld* GetWorld() const override;
};

