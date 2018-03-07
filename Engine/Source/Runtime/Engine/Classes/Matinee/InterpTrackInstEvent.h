// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "InterpTrackInstEvent.generated.h"

class UInterpTrack;

UCLASS(MinimalAPI)
class UInterpTrackInstEvent : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	Position we were in last time we evaluated Events. 
	 *	During UpdateTrack, events between this time and the current time will be fired. 
	 */
	UPROPERTY()
	float LastUpdatePosition;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

