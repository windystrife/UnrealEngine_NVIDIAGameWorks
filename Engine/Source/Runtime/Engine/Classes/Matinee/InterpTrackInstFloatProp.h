// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInstProperty.h"
#include "InterpTrackInstFloatProp.generated.h"

class UInterpTrack;

UCLASS(MinimalAPI)
class UInterpTrackInstFloatProp : public UInterpTrackInstProperty
{
	GENERATED_UCLASS_BODY()

	/** Pointer to float property in TrackObject. */
	float* FloatProp;

	/** Saved value for restoring state when exiting Matinee. */
	UPROPERTY()
	float ResetFloat;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

