// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "InterpTrackInstSound.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstSound : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	float LastUpdatePosition;

	UPROPERTY(transient)
	class UAudioComponent* PlayAudioComp;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void TermTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

