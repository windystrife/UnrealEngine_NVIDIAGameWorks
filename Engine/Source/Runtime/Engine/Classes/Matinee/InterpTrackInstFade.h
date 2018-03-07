// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "InterpTrackInstFade.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstFade : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()


	// Begin UInterpTrackInst Instance
	virtual void TermTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

