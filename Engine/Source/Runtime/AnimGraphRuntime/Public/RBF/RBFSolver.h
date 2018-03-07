// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimTypes.h"
#include "Curves/RichCurve.h"
#include "RBFSolver.generated.h"

/** Function to use for each target falloff */
UENUM()
enum class ERBFFunctionType : uint8
{
	Gaussian,

	Exponential,

	Linear,

	Cubic,

	Quintic
};

/** Method for determining distance from input to targets */
UENUM()
enum class ERBFDistanceMethod : uint8
{
	/** Standard n-dimensional distance measure */
	Euclidean,

	/** Treat inputs as quaternion */
	Quaternion,

	/** Treat inputs as quaternion, and find distance between rotated TwistAxis direction */
	SwingAngle
};

/** Struct storing a particular entry within the RBF */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FRBFEntry
{
	GENERATED_BODY()

	/** Set of values for this target, size must be TargetDimensions  */
	UPROPERTY(EditAnywhere, Category = RBFData)
	TArray<float> Values;

	/** Return a target as a quaternion, assuming Values is a sequence of Euler entries. Index is which Euler to convert. */
	FQuat AsQuat(int32 Index) const;
	/** Set this entry to 3 floats from supplied rotator */
	void AddFromRotator(const FRotator& InRot);
	/** Set this entry to 3 floats from supplied vector */
	void AddFromVector(const FVector& InVector);

	/** Return dimensionality of this target */
	int32 GetDimensions() const
	{
		return Values.Num();
	}
};

/** Data about a particular target in the RBF, including scaling factor */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FRBFTarget : public FRBFEntry
{
	GENERATED_BODY()

	/** How large to scale */
	UPROPERTY(EditAnywhere, Category = RBFData)
	float ScaleFactor;

	/** Whether we want to apply an additional custom curve when activating this target */
	UPROPERTY(EditAnywhere, Category = RBFData)
	bool bApplyCustomCurve;

	/** Custom curve to apply to activation of this target, if bApplyCustomCurve is true */
	UPROPERTY(EditAnywhere, Category = RBFData)
	FRichCurve CustomCurve;

	FRBFTarget()
		: ScaleFactor(1.f)
		, bApplyCustomCurve(false)
	{}
};

/** Struct for storing RBF results - target index and corresponding weight */
struct ANIMGRAPHRUNTIME_API FRBFOutputWeight
{
	/** Index of target */
	int32 TargetIndex;
	/** Weight of target */
	float TargetWeight;

	FRBFOutputWeight(int32 InTargetIndex, float InTargetWeight)
		: TargetIndex(InTargetIndex)
		, TargetWeight(InTargetWeight)
	{}

	FRBFOutputWeight()
		: TargetIndex(0)
		, TargetWeight(0.f)
	{}
};

/** Parameters used by RBF solver */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FRBFParams
{
	GENERATED_BODY()

	/** How many dimensions input data has */
	UPROPERTY()
	int32 TargetDimensions;

	/** Default radius for each target, scaled by per-Target ScaleFactor */
	UPROPERTY(EditAnywhere, Category = RBFData)
	float Radius;

	UPROPERTY(EditAnywhere, Category = RBFData)
	ERBFFunctionType Function;

	UPROPERTY(EditAnywhere, Category = RBFData)
	ERBFDistanceMethod DistanceMethod;

	/** Axis to use when DistanceMethod is SwingAngle */
	UPROPERTY(EditAnywhere, Category = RBFData)
	TEnumAsByte<EBoneAxis> TwistAxis;

	/** Weight below which we shouldn't bother returning a contribution from a target */
	UPROPERTY(EditAnywhere, Category = RBFData)
	float WeightThreshold;

	FRBFParams();

	/** Util for returning unit direction vector for swing axis */
	FVector GetTwistAxisVector() const;
};

/** Library of Radial Basis Function solver functions */
struct ANIMGRAPHRUNTIME_API FRBFSolver
{
	/** Given a set of targets and new input entry, give list of activated targets with weights */
	static void Solve(const FRBFParams& Params, const TArray<FRBFTarget>& Targets, const FRBFEntry& Input, TArray<FRBFOutputWeight>& OutputWeights);

	/** Util to find distance to nearest neighbour target for each target */
	static bool FindTargetNeighbourDistances(const FRBFParams& Params, const TArray<FRBFTarget>& Targets, TArray<float>& NeighbourDists);

	/** Util to find distance between two entries, using provided params */
	static float FindDistanceBetweenEntries(const FRBFEntry& A, const FRBFEntry& B, const FRBFParams& Params);
};
