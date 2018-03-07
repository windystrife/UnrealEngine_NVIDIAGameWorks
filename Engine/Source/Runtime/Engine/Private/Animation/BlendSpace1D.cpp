// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BlendSpace1D.cpp: 1D BlendSpace functionality
=============================================================================*/ 

#include "Animation/BlendSpace1D.h"

UBlendSpace1D::UBlendSpace1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBlendSpace1D::IsValidAdditive() const
{
	return ContainsMatchingSamples(AAT_LocalSpaceBase) || ContainsMatchingSamples(AAT_RotationOffsetMeshSpace);
}

bool UBlendSpace1D::IsValidAdditiveType(EAdditiveAnimationType AdditiveType) const
{
	return (AdditiveType == AAT_LocalSpaceBase || AdditiveType == AAT_RotationOffsetMeshSpace || AdditiveType == AAT_None);
}

EBlendSpaceAxis UBlendSpace1D::GetAxisToScale() const
{
	return bScaleAnimation ? BSA_X : BSA_None;
}

bool UBlendSpace1D::IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const
{
	return FMath::IsNearlyEqual(SamplePointA.X, SamplePointB.X);
}

void UBlendSpace1D::GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample, TInlineAllocator<4> > & OutBlendSamples) const
{

	FVector NormalizedBlendInput = GetNormalizedBlendInput(BlendInput);

	float GridIndex = FMath::TruncToFloat(NormalizedBlendInput.X);
	float Remainder = NormalizedBlendInput.X - GridIndex;

	const FEditorElement* BeforeElement = GetGridSampleInternal(GridIndex);

	if (BeforeElement)
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = *BeforeElement;
		// now calculate weight - GridElement has weights to nearest samples, here we weight the grid element
		NewSample.BlendWeight = (1.f-Remainder);
		OutBlendSamples.Add(NewSample);
	}
	else
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = FEditorElement();
		NewSample.BlendWeight = 0.f;
		OutBlendSamples.Add(NewSample);
	}

	const FEditorElement* AfterElement = GetGridSampleInternal(GridIndex+1);

	if (AfterElement)
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = *AfterElement;
		// now calculate weight - GridElement has weights to nearest samples, here we weight the grid element
		NewSample.BlendWeight = (Remainder);
		OutBlendSamples.Add(NewSample);
	}
	else
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = FEditorElement();
		NewSample.BlendWeight = 0.f;
		OutBlendSamples.Add(NewSample);
	}
}

#if WITH_EDITOR
void UBlendSpace1D::SnapSamplesToClosestGridPoint()
{
	TArray<float> GridPoints;
	TArray<int32> ClosestSampleToGridPoint;
	TArray<bool> SampleDataOnPoints;

	const float GridMin = BlendParameters[0].Min;
	const float GridMax = BlendParameters[0].Max;
	const float GridRange = GridMax - GridMin;
	const int32 NumGridPoints = BlendParameters[0].GridNum + 1;
	const float GridStep = GridRange / BlendParameters[0].GridNum;

	// First mark all samples as invalid
	for (FBlendSample& BlendSample : SampleData)
	{
		BlendSample.bIsValid = false;
	}

	for (int32 GridPointIndex = 0; GridPointIndex < NumGridPoints; ++GridPointIndex)
	{
		const float GridPointValue = (GridPointIndex * GridStep) + GridMin;
		GridPoints.Add(GridPointValue);
	}

	ClosestSampleToGridPoint.Init(INDEX_NONE, GridPoints.Num());

	// Find closest sample to grid point
	for (int32 PointIndex = 0; PointIndex < GridPoints.Num(); ++PointIndex)
	{
		const float GridPoint = GridPoints[PointIndex];
		float SmallestDistance = FLT_MAX;
		int32 Index = INDEX_NONE;

		for (int32 SampleIndex = 0; SampleIndex < SampleData.Num(); ++SampleIndex)
		{
			FBlendSample& BlendSample = SampleData[SampleIndex];
			const float Distance = FMath::Abs(GridPoint - BlendSample.SampleValue.X);
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
			const float Distance = FMath::Abs(GridPoints[PointIndex] - BlendSample.SampleValue.X);
			if (Distance < SmallestDistance)
			{
				Index = PointIndex;
				SmallestDistance = Distance;
			}
		}

		// Only move the sample if it is also closest to the grid point
		if (Index != INDEX_NONE && ClosestSampleToGridPoint[Index] == SampleIndex)
		{
			BlendSample.SampleValue.X = GridPoints[Index];
			BlendSample.bIsValid = true;
		}
	}
}

void UBlendSpace1D::RemapSamplesToNewAxisRange()
{
	const float OldGridMin = PreviousAxisMinMaxValues[0].X;
	const float OldGridMax = PreviousAxisMinMaxValues[0].Y;
	const float OldGridRange = OldGridMax - OldGridMin;

	const float NewGridMin = BlendParameters[0].Min;
	const float NewGridMax = BlendParameters[0].Max;
	const float NewGridRange = NewGridMax - NewGridMin;

	for (FBlendSample& BlendSample : SampleData)
	{
		const float NormalizedValue = (BlendSample.SampleValue.X - OldGridMin) / OldGridRange;		
		BlendSample.SampleValue.X = NewGridMin + (NormalizedValue * NewGridRange);
	}
}
#endif // WITH_EDITOR
