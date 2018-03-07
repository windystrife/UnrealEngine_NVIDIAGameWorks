// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/BlendSpaceBase.h"

#include "BlendSpace.generated.h"

/**
 * Contains a grid of data points with weights from sample points in the space
 */
UCLASS(config=Engine, hidecategories=Object, MinimalAPI, BlueprintType)
class UBlendSpace : public UBlendSpaceBase
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool IsValidAdditiveType(EAdditiveAnimationType AdditiveType) const override;
	virtual bool IsValidAdditive() const override;
protected:
	//~ Begin UBlendSpaceBase Interface
	virtual EBlendSpaceAxis GetAxisToScale() const override;
	virtual bool IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const;
	virtual void GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample, TInlineAllocator<4> > & OutBlendSamples) const override;
#if WITH_EDITOR
	virtual void SnapSamplesToClosestGridPoint();
	virtual void RemapSamplesToNewAxisRange() override;
#endif // WITH_EDITOR
	//~ End UBlendSpaceBase Interface

	/**
	* Get Grid Samples from BlendInput, From Input, it will return the 4 points of the grid that this input belongs to.
	*
	* @param	BlendInput	BlendInput X, Y, Z corresponds to BlendParameters[0], [1], [2]
	*
	* @return	LeftTop, RightTop, LeftBottom, RightBottom	4 corner of the grid this BlendInput belongs to
	*			It's possible they return INDEX_NONE, but that case the weight also should be 0.f
	*
	*/
	void GetGridSamplesFromBlendInput(const FVector &BlendInput, FGridBlendSample & LeftBottom, FGridBlendSample & RightBottom, FGridBlendSample & LeftTop, FGridBlendSample& RightTop) const;

	/** Get the Editor Element from Index
	*
	* @param	XIndex	Index of X
	* @param	YIndex	Index of Y
	*
	* @return	FEditorElement * return the grid data
	*/
	const FEditorElement* GetEditorElement(int32 XIndex, int32 YIndex) const;
protected:
	/** If you have input interpolation, which axis to drive animation speed (scale) - i.e. for locomotion animation, speed axis will drive animation speed (thus scale)**/
	UPROPERTY(EditAnywhere, Category = InputInterpolation)
	TEnumAsByte<EBlendSpaceAxis> AxisToScaleAnimation;
};
