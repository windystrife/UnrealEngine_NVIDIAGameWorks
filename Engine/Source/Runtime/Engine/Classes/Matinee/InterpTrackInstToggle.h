// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "Matinee/InterpTrackToggle.h"

#include "InterpTrackInstToggle.generated.h"

UCLASS()
class UInterpTrackInstToggle : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=InterpTrackInstToggle)
	TEnumAsByte<enum ETrackToggleAction> Action;

	/** 
	 *	Position we were in last time we evaluated.
	 *	During UpdateTrack, toggles between this time and the current time will be processed.
	 */
	UPROPERTY()
	float LastUpdatePosition;

	/** Cached 'active' state for the toggleable actor before we possessed it; restored when Matinee exits */
	UPROPERTY()
	uint32 bSavedActiveState:1;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

