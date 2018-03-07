// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "Components/SplineComponent.h"
#include "ControlRig.h"
#include "AlphaBlend.h"
#include "SplineIKControlRig.generated.h"

/** ControlRig that handles spline IK */
UCLASS(BlueprintType, editinlinenew, meta=(DisplayName="Spline IK"))
class CONTROLRIG_API USplineIKControlRig : public UControlRig
{
	GENERATED_BODY()

	USplineIKControlRig();

	/** Spline component - set via accessor */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	USplineComponent* SplineComponent;

	/** Input transforms */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	TArray<FTransform> InputTransforms;

	/** Output transforms */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationOutput))
	TArray<FTransform> OutputTransforms;

	/** Axis of the controlled bone (ie the direction of the spline) to use as the direction for the curve. */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	TEnumAsByte<EAxis::Type> BoneAxis;

	/** Overall roll of the spline, applied on top of other rotations along the direction of the spline */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	float Roll;

	/** The twist of the start bone. Twist is interpolated along the spline according to Twist Blend. */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	float TwistStart;

	/** The twist of the end bone. Twist is interpolated along the spline according to Twist Blend. */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	float TwistEnd;

	/** How to interpolate twist along the length of the spline */
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FAlphaBlend TwistBlend;

	/**
	 * The maximum stretch allowed when fitting bones to the spline. 0.0 means bones do not stretch their length,
	 * 1.0 means bones stretch to the length of the spline
	 */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	float Stretch;

	/** The distance along the spline from the start from which bones are constrained */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (AnimationInput))
	float Offset;

	/** Setter for SplineComponent - copies out spline points, rebuilds linear approximation */
	UFUNCTION(BlueprintCallable, Category = "Spline IK")
	void SetSplineComponent(USplineComponent* InSplineComponent);

	// UControlRig interface
#if WITH_EDITOR
	virtual FText GetCategory() const override;
	virtual FText GetTooltipText() const override;
#endif

	// IControlRigInterface interface
	virtual void Evaluate() override;

private:
	/** Get the current twist value at the specified spline alpha */
	float GetTwist(float Alpha, float TotalSplineAlpha);

	/** Use our linear approximation to determine the earliest intersection with a sphere */
	float FindParamAtFirstSphereIntersection(const FVector& InOrigin, float InRadius, int32& StartingLinearIndex);

private:
	/** Dirty flag - set when the spline component is re-set via SetSplineComponent */
	bool bDirty;

	/** Spline curves we use orient nodes */
	FSplineCurves SplineCurves;

	/** Piecewise linear approximation of the spline, recalculated on deformation */
	TArray<FSplinePositionLinearApproximation> LinearApproximation;

	/** Cached spline length from when the spline was originally applied to the ControlRig. Stretch is applied using the difference of between this and the current spline length */
	float OriginalSplineLength;

	/** Dirty flag for original spline length */
	bool bHaveOriginalSplineLength;

	/** Cached data for nodes in the IK chain, from start to end */
	UPROPERTY()
	TArray<FName> CachedNodeNames;

	/** Cached bone lengths. Same size as CachedNodeNames */
	UPROPERTY()
	TArray<float> CachedBoneLengths;

	/** Cached bone offset rotations. Same size as CachedNodeNames */
	UPROPERTY()
	TArray<FQuat> CachedOffsetRotations;
};