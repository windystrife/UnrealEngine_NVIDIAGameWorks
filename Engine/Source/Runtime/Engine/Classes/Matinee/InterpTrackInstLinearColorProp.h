// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInstProperty.h"
#include "InterpTrackInstLinearColorProp.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstLinearColorProp : public UInterpTrackInstProperty
{
	GENERATED_UCLASS_BODY()

	/** Pointer to color property in TrackObject. */
	FLinearColor* ColorProp;

	/** Saved value for restoring state when exiting Matinee. */
	UPROPERTY()
	FLinearColor ResetColor;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

