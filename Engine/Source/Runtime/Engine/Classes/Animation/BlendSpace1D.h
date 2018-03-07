// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Blend Space 1D. Contains 1 axis blend 'space'
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/BlendSpaceBase.h"
#include "BlendSpace1D.generated.h"

UCLASS(config=Engine, hidecategories=Object, MinimalAPI, BlueprintType)
class UBlendSpace1D : public UBlendSpaceBase
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bDisplayEditorVertically_DEPRECATED;
#endif

	/** Drive animation speed by blend input position **/
	UPROPERTY()
	bool bScaleAnimation;

	virtual bool IsValidAdditive() const override;
	virtual bool IsValidAdditiveType(EAdditiveAnimationType AdditiveType) const override;
protected:
	//~ Begin UBlendSpaceBase Interface
	virtual bool IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const;	
	virtual EBlendSpaceAxis GetAxisToScale() const override;
	virtual void GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample, TInlineAllocator<4> > & OutBlendSamples) const override;
#if WITH_EDITOR
	virtual void SnapSamplesToClosestGridPoint() override;
	virtual void RemapSamplesToNewAxisRange() override;
#endif // WITH_EDITOR
	//~ End UBlendSpaceBase Interface
};
