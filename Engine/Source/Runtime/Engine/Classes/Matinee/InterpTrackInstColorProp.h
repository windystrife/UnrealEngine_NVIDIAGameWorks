// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInstProperty.h"
#include "InterpTrackInstColorProp.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstColorProp : public UInterpTrackInstProperty
{
	GENERATED_UCLASS_BODY()

	/** Pointer to color property in TrackObject. */
	FColor* ColorProp;

	/** Saved value for restoring state when exiting Matinee. */
	UPROPERTY()
	FColor ResetColor;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

