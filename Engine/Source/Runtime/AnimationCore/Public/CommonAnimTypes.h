// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationTypes.h: Render core module definitions.
=============================================================================*/

#pragma once
#include "CoreMinimal.h"
#include "CommonAnimTypes.generated.h"

/** Axis to represent direction */
USTRUCT()
struct FAxis
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "FAxis")
	FVector Axis;

	UPROPERTY(EditAnywhere, Category = "FAxis")
	bool bInLocalSpace;

	FAxis(const FVector& InAxis = FVector::ForwardVector)
		: Axis(InAxis)
		, bInLocalSpace(true) {};

	/** return transformed axis based on ComponentSpaceTransform */
	FVector GetTransformedAxis(const FTransform& ComponentSpaceTransform) const
	{
		if (bInLocalSpace)
		{
			return ComponentSpaceTransform.TransformVectorNoScale(Axis);
		}

		// if world transform, we don't have to transform
		return Axis;
	}

	/** Initialize the set up */
	void Initialize()
	{
		Axis = Axis.GetSafeNormal();
	}

	/** return true if Valid data */
	bool IsValid() const
	{
		return Axis.IsNormalized();
	}

	friend FArchive & operator<<(FArchive & Ar, FAxis & D)
	{
		Ar << D.Axis;
		Ar << D.bInLocalSpace;

		return Ar;
	}
};

