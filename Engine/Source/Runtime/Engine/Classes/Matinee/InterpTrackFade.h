// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackFade.generated.h"

class UInterpTrackInst;

UCLASS(meta=( DisplayName = "Fade Track" ) )
class UInterpTrackFade : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()

	/** 
	 * InterpTrackFade
	 *
	 * Special float property track that controls camera fading over time.
	 * Should live in a Director group.
	 *
	 */
	UPROPERTY(EditAnywhere, Category=InterpTrackFade)
	uint32 bPersistFade:1;

	/** True to set master audio volume along with the visual fade. */
	UPROPERTY(EditAnywhere, Category = InterpTrackFade)
	uint32 bFadeAudio : 1;

	/** Color to fade to. */
	UPROPERTY(EditAnywhere, Category = InterpTrackFade)
	FLinearColor FadeColor;

	//~ Begin UInterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	//~ End UInterpTrack Interface.

	/** @return the amount of fading we want at the given time. */
	ENGINE_API float GetFadeAmountAtTime(float Time);
};



