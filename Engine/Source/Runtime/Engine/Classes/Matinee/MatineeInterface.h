// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/**
 * This interface implements Matinee related functions
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "MatineeInterface.generated.h"

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class UMatineeInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IMatineeInterface
{
	GENERATED_IINTERFACE_BODY()


	/** Called when a matinee actor starts interpolating this  AActor via matinee.
	 * @note this function is called on clients for actors that are interpolated clientside via MatineeActor
	 * @param MatineeActor the MatineeActor that was affecting the Actor
	 */
	virtual void InterpolationStarted(class AMatineeActor* MatineeActor)=0;
	

	/** Called when a matinee actor finished interpolating this Actor.
	 * @note this function is called on clients for actors that are interpolated clientside via MatineeActor
	 * @param MatineeActor the MatineeActor that was affecting the Actor
	 */
	virtual void InterpolationFinished(class AMatineeActor* MatineeActor)=0;
	

	/** Called when a matinee actor affecting this  AActor  received an event that changed its properties
	 *	(paused, reversed direction, etc).
	 * @note this function is called on clients for actors that are interpolated clientside via MatineeActor
	 * @param MatineeActor the MatineeActor that is affecting the Actor
	 */
	virtual void InterpolationChanged(class AMatineeActor* MatineeActor)=0;
};

