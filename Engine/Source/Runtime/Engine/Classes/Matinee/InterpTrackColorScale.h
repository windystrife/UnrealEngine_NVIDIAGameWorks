// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 * InterpTrackColorScale
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "InterpTrackColorScale.generated.h"

class UInterpTrackInst;

UCLASS(meta=( DisplayName = "Color Scale Track" ) )
class UInterpTrackColorScale : public UInterpTrackVectorBase
{
	GENERATED_UCLASS_BODY()


	//~ Begin UInterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	virtual void SetTrackToSensibleDefault() override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	//~ Begin UInterpTrack Interface.

	/** Return the blur alpha we want at the given time. */
	ENGINE_API FVector GetColorScaleAtTime(float Time);
};



