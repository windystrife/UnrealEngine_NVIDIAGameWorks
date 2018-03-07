// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackFloatMaterialParam.generated.h"

class UInterpTrackInst;
struct FPropertyChangedEvent;

UCLASS(meta=( DisplayName = "Float Material Parameter Track" ) )
class UInterpTrackFloatMaterialParam : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()
	
	/** Materials whose parameters we want to change and the references to those materials. */
	UPROPERTY(EditAnywhere, Category=InterpTrackFloatMaterialParam)
	TArray<class UMaterialInterface*> TargetMaterials;

	/** Name of parameter in the MaterialInstance which this track will modify over time. */
	UPROPERTY(EditAnywhere, Category=InterpTrackFloatMaterialParam)
	FName ParamName;


	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UInterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	//~ End UInterpTrack Interface.
};



