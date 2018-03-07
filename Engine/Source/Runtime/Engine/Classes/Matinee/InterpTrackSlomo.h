// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackSlomo.generated.h"

class UInterpTrackInst;

UCLASS(meta=( DisplayName = "Slomo Track" ) )
class UInterpTrackSlomo : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()


	//~ Begin InterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	virtual void SetTrackToSensibleDefault() override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	//~ End InterpTrack Interface.

	/** @return the slomo factor we want at the given time. */
	ENGINE_API float GetSlomoFactorAtTime(float Time);
};



