// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInstProperty.h"
#include "InterpTrackInstBoolProp.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstBoolProp : public UInterpTrackInstProperty
{
	GENERATED_UCLASS_BODY()

	/** Pointer to boolean property in TrackObject. */
	void* BoolPropertyAddress;

	/** Mask that indicates which bit the boolean property actually uses of the value pointed to by BoolProp. */
	UPROPERTY(transient)
	UBoolProperty* BoolProperty;

	/** Saved value for restoring state when exiting Matinee. */
	UPROPERTY()
	bool ResetBool;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

