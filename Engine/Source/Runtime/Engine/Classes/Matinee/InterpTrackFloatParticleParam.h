// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackFloatParticleParam.generated.h"

class UInterpTrackInst;

UCLASS(meta=( DisplayName = "Float Particle Param Track" ) )
class UInterpTrackFloatParticleParam : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()
	
	/** Name of property in the Emitter which this track mill modify over time. */
	UPROPERTY(EditAnywhere, Category=InterpTrackFloatParticleParam)
	FName ParamName;


	//~ Begin UInterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	//~ End UInterpTrack Interface.
};



