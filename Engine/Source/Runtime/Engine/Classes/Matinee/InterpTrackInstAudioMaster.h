// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "InterpTrackInstAudioMaster.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstAudioMaster : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

