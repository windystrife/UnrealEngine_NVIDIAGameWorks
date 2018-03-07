// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/MatineeActor.h"
#include "MatineeActorCameraAnim.generated.h"

class UCameraAnim;

/**
 * Actor used to control temporary matinees for camera anims that only exist in the editor
 */
UCLASS(notplaceable, MinimalAPI, NotBlueprintable)
class AMatineeActorCameraAnim : public AMatineeActor
{
	GENERATED_UCLASS_BODY()

	/** The camera anim we are editing */
	UPROPERTY(Transient)
	UCameraAnim* CameraAnim;
};
