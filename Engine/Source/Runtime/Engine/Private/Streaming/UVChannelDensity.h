// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
UVChannelDensity.h: Helpers to compute UV channel density.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITORONLY_DATA

struct FUVDensityAccumulator
{
private:

	struct FElementInfo
	{
		float Weight;
		float UVDensity;
		FElementInfo(float InWeight, float InUVDensity) : Weight(InWeight), UVDensity(InUVDensity) {}
	};

	TArray<FElementInfo> Elements;

public:

	void Reserve(int32 Size) { Elements.Reserve(Size); }

	void PushTriangle(float InAera, float InUVArea)
	{
		if (InAera > SMALL_NUMBER && InUVArea > SMALL_NUMBER)
		{
			Elements.Add(FElementInfo(FMath::Sqrt(InAera), FMath::Sqrt(InAera / InUVArea)));
		}
	}

	void AccumulateDensity(float& WeightedUVDensity, float& Weight, float DiscardPercentage = .10f)
	{
		struct FCompareUVDensity
		{
			FORCEINLINE bool operator()(FElementInfo const& A, FElementInfo const& B) const { return A.UVDensity < B.UVDensity; }
		};

		Elements.Sort(FCompareUVDensity());

		// Remove 10% of higher and lower texel factors.
		const int32 Threshold = FMath::FloorToInt(DiscardPercentage * (float)Elements.Num());
		for (int32 Index = Threshold; Index < Elements.Num() - Threshold; ++Index)
		{
			const FElementInfo& Element = Elements[Index];
			WeightedUVDensity += Element.UVDensity * Element.Weight;
			Weight += Element.Weight;
		}
	}

	float GetDensity(float DiscardPercentage = .10f)
	{
		float WeightedUVDensity = 0;
		float Weight = 0;

		AccumulateDensity(WeightedUVDensity, Weight, DiscardPercentage);

		return (Weight > SMALL_NUMBER) ? (WeightedUVDensity / Weight) : 0;
	}

	FORCEINLINE_DEBUGGABLE 
	static float GetTriangleAera(const FVector& Pos0, const FVector& Pos1, const FVector& Pos2)
	{
		FVector P01 = Pos1 - Pos0;
		FVector P02 = Pos2 - Pos0;
		return FVector::CrossProduct(P01, P02).Size();
	}

	FORCEINLINE_DEBUGGABLE 
	static float GetUVChannelAera(const FVector2D& UV0, const FVector2D& UV1, const FVector2D& UV2)
	{
		FVector2D UV01 = UV1 - UV0;
		FVector2D UV02 = UV2 - UV0;
		return FMath::Abs<float>(UV01.X * UV02.Y - UV01.Y * UV02.X);
	}
};

#endif
