// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "InterpTrackInstFloatParticleParam.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstFloatParticleParam : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/** Saved value for restoring state when exiting Matinee. */
	UPROPERTY()
	float ResetFloat;


	// Begin UInterpTrackInst Instance
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

