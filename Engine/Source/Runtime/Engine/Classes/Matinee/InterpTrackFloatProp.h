// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackFloatProp.generated.h"

class UInterpTrackInst;

UCLASS(MinimalAPI, meta=( DisplayName = "Float Property Track" ) )
class UInterpTrackFloatProp : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()
	
	/** Name of property in Group  AActor  which this track mill modify over time. */
	UPROPERTY(Category=InterpTrackFloatProp, VisibleAnywhere)
	FName PropertyName;


	//~ Begin InterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual bool CanAddKeyframe( UInterpTrackInst* TrackInst ) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	virtual const FString	GetEdHelperClassName() const override;
	virtual const FString	GetSlateHelperClassName() const override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	virtual void ReduceKeys( float IntervalStart, float IntervalEnd, float Tolerance ) override;
	//~ End InterpTrack Interface.
};



