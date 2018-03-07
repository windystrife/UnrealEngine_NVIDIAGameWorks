// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BlendSpace.cpp: 2D BlendSpace functionality
=============================================================================*/ 

#include "Animation/BlendSpace.h"

UBlendSpace::UBlendSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlendSpace::GetGridSamplesFromBlendInput(const FVector &BlendInput, FGridBlendSample & LeftBottom, FGridBlendSample & RightBottom, FGridBlendSample & LeftTop, FGridBlendSample& RightTop) const
{
	const FVector NormalizedBlendInput = GetNormalizedBlendInput(BlendInput);	
	const FVector GridIndex(FMath::TruncToFloat(NormalizedBlendInput.X), FMath::TruncToFloat(NormalizedBlendInput.Y), 0.f);
	const FVector Remainder = NormalizedBlendInput - GridIndex;
	// bi-linear very simple interpolation
	const FEditorElement* EleLT = GetEditorElement(GridIndex.X, GridIndex.Y+1);
	if (EleLT)
	{
		LeftTop.GridElement = *EleLT;
		// now calculate weight - distance to each corner since input is already normalized within grid, we can just calculate distance 
		LeftTop.BlendWeight = (1.f-Remainder.X)*Remainder.Y;
	}
	else
	{
		LeftTop.GridElement = FEditorElement();
		LeftTop.BlendWeight = 0.f;
	}

	const FEditorElement* EleRT = GetEditorElement(GridIndex.X+1, GridIndex.Y+1);
	if (EleRT)
	{
		RightTop.GridElement = *EleRT;
		RightTop.BlendWeight = Remainder.X*Remainder.Y;
	}
	else
	{
		RightTop.GridElement = FEditorElement();
		RightTop.BlendWeight = 0.f;
	}

	const FEditorElement* EleLB = GetEditorElement(GridIndex.X, GridIndex.Y);
	if (EleLB)
	{
		LeftBottom.GridElement = *EleLB;
		LeftBottom.BlendWeight = (1.f-Remainder.X)*(1.f-Remainder.Y);
	}
	else
	{
		LeftBottom.GridElement = FEditorElement();
		LeftBottom.BlendWeight = 0.f;
	}

	const FEditorElement* EleRB = GetEditorElement(GridIndex.X+1, GridIndex.Y);
	if (EleRB)
	{
		RightBottom.GridElement = *EleRB;
		RightBottom.BlendWeight = Remainder.X*(1.f-Remainder.Y);
	}
	else
	{
		RightBottom.GridElement = FEditorElement();
		RightBottom.BlendWeight = 0.f;
	}
}

void UBlendSpace::GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample, TInlineAllocator<4> > & OutBlendSamples) const
{
	OutBlendSamples.Reset();
	OutBlendSamples.AddUninitialized(4);

	GetGridSamplesFromBlendInput(BlendInput, OutBlendSamples[0], OutBlendSamples[1], OutBlendSamples[2], OutBlendSamples[3]);
}

const FEditorElement* UBlendSpace::GetEditorElement(int32 XIndex, int32 YIndex) const
{
	int32 Index = XIndex*(BlendParameters[1].GridNum+1) + YIndex;
	return GetGridSampleInternal(Index);
}

bool UBlendSpace::IsValidAdditiveType(EAdditiveAnimationType AdditiveType) const
{
	return (AdditiveType == AAT_LocalSpaceBase || AdditiveType == AAT_RotationOffsetMeshSpace || AdditiveType == AAT_None);
}

bool UBlendSpace::IsValidAdditive() const
{
	return ContainsMatchingSamples(AAT_LocalSpaceBase) || ContainsMatchingSamples(AAT_RotationOffsetMeshSpace);
}

#if WITH_EDITOR
void UBlendSpace::SnapSamplesToClosestGridPoint()
{
	TArray<FVector> GridPoints;
	TArray<int32> ClosestSampleToGridPoint;
	TArray<bool> SampleDataOnPoints;

	const FVector GridMin(BlendParameters[0].Min, BlendParameters[1].Min, 0.0f);
	const FVector GridMax(BlendParameters[0].Max, BlendParameters[1].Max, 0.0f);
	const FVector GridRange(GridMax.X - GridMin.X, GridMax.Y - GridMin.Y, 0.0f);
	const FIntPoint NumGridPoints(BlendParameters[0].GridNum + 1, BlendParameters[1].GridNum + 1);
	const FVector GridStep(GridRange.X / BlendParameters[0].GridNum, GridRange.Y / BlendParameters[1].GridNum, 0.0f);
	
	// First mark all samples as invalid
	for (FBlendSample& BlendSample : SampleData)
	{
			BlendSample.bIsValid = false;
	}

	for (int32 GridY = 0; GridY < NumGridPoints.Y; ++GridY)
	{
		for (int32 GridX = 0; GridX < NumGridPoints.X; ++GridX)
		{
			const FVector GridPoint((GridX * GridStep.X) + GridMin.X, (GridY * GridStep.Y) + GridMin.Y, 0.0f);
			GridPoints.Add(GridPoint);
		}
	}

	ClosestSampleToGridPoint.Init(INDEX_NONE, GridPoints.Num());

	// Find closest sample to grid point
	for (int32 PointIndex = 0; PointIndex < GridPoints.Num(); ++PointIndex)
	{
		const FVector& GridPoint = GridPoints[PointIndex];
		float SmallestDistance = FLT_MAX;
		int32 Index = INDEX_NONE;

		for (int32 SampleIndex = 0; SampleIndex < SampleData.Num(); ++SampleIndex)
		{
			FBlendSample& BlendSample = SampleData[SampleIndex];
			const float Distance = (GridPoint - BlendSample.SampleValue).SizeSquared2D();
			if (Distance < SmallestDistance)
			{
				Index = SampleIndex;
				SmallestDistance = Distance;
			}
		}

		ClosestSampleToGridPoint[PointIndex] = Index;
	}

	// Find closest grid point to sample
	for (int32 SampleIndex = 0; SampleIndex < SampleData.Num(); ++SampleIndex)
	{
		FBlendSample& BlendSample = SampleData[SampleIndex];

		// Find closest grid point
		float SmallestDistance = FLT_MAX;
		int32 Index = INDEX_NONE;
		for (int32 PointIndex = 0; PointIndex < GridPoints.Num(); ++PointIndex)
		{
			const float Distance = (GridPoints[PointIndex] - BlendSample.SampleValue).SizeSquared2D();
			if (Distance < SmallestDistance)
			{
				Index = PointIndex;
				SmallestDistance = Distance;
			}
		}

		// Only move the sample if it is also closest to the grid point
		if (Index != INDEX_NONE && ClosestSampleToGridPoint[Index] == SampleIndex)
		{
			BlendSample.SampleValue = GridPoints[Index];
			BlendSample.bIsValid = true;
		}
	}
}

void UBlendSpace::RemapSamplesToNewAxisRange()
{
	const FVector OldGridMin(PreviousAxisMinMaxValues[0].X, PreviousAxisMinMaxValues[1].X, 0.0f);
	const FVector OldGridMax(PreviousAxisMinMaxValues[0].Y, PreviousAxisMinMaxValues[1].Y, 1.0f);
	const FVector OldGridRange = OldGridMax - OldGridMin;

	const FVector NewGridMin(BlendParameters[0].Min, BlendParameters[1].Min, 0.0f);
	const FVector NewGridMax(BlendParameters[0].Max, BlendParameters[1].Max, 1.0f);
	const FVector NewGridRange = NewGridMax - NewGridMin;

	for (FBlendSample& BlendSample : SampleData)
	{
		const FVector NormalizedValue = (BlendSample.SampleValue - OldGridMin) / OldGridRange;
		BlendSample.SampleValue = NewGridMin + (NormalizedValue * NewGridRange);
	}
}
#endif // WITH_EDITOR

EBlendSpaceAxis UBlendSpace::GetAxisToScale() const
{
	return AxisToScaleAnimation;
}

bool UBlendSpace::IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const
{
	return FMath::IsNearlyEqual(SamplePointA.X, SamplePointB.X) && FMath::IsNearlyEqual(SamplePointA.Y, SamplePointB.Y);
}
