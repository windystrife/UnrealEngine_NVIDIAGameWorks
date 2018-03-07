// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackFloatAnimBPParam.generated.h"

class UAnimInstance;
class UInterpTrackInst;
struct FPropertyChangedEvent;

UCLASS(meta=( DisplayName = "Float Anim BP Parameter Track" ) )
class UInterpTrackFloatAnimBPParam : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()
	
	DEPRECATED(4.11, "This property is deprecated. Please use AnimClass instead")
	UPROPERTY(EditAnywhere, Category = InterpTrackFloatAnimBPParam)
	class UAnimBlueprintGeneratedClass* AnimBlueprintClass;

	/** Materials whose parameters we want to change and the references to those materials. */
	UPROPERTY(EditAnywhere, Category=InterpTrackFloatAnimBPParam)
	TSubclassOf<UAnimInstance> AnimClass;

	/** Name of parameter in the MaterialInstance which this track will modify over time. */
	UPROPERTY(EditAnywhere, Category=InterpTrackFloatAnimBPParam)
	FName ParamName;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UInterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	//~ End UInterpTrack Interface.

private:
	bool bRefreshParamter;
};



