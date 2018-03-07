// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SplineIKControlRig.h"
#include "SplineIK.h"

#define LOCTEXT_NAMESPACE "SplineIKControlRig"

USplineIKControlRig::USplineIKControlRig()
	: BoneAxis(EAxis::X)
	, Roll(0.0f)
	, TwistStart(0.0f)
	, TwistEnd(0.0f)
	, Stretch(0.0f)
	, Offset(0.0f)
	, bDirty(false)
	, OriginalSplineLength(0.0f)
	, bHaveOriginalSplineLength(false)
{
}

void USplineIKControlRig::SetSplineComponent(USplineComponent* InSplineComponent)
{
	if (InSplineComponent)
	{
		// copy spline curves
		SplineCurves = InSplineComponent->SplineCurves;
		bDirty = true;

		// copy spline length if this is the first use of this setup
		if (!bHaveOriginalSplineLength)
		{
			OriginalSplineLength = InSplineComponent->GetSplineLength();
			bHaveOriginalSplineLength = true;
		}
	}
}

#if WITH_EDITOR
FText USplineIKControlRig::GetCategory() const
{
	return LOCTEXT("SplineIKCategory", "Animation|Constraints");
}

FText USplineIKControlRig::GetTooltipText() const
{
	return LOCTEXT("SplineIKTooltip", "Constrains input nodes to a spline.");
}
#endif

void USplineIKControlRig::Evaluate()
{
	if (bDirty)
	{
		// rebuild linear approximation if dirty
		FSplinePositionLinearApproximation::Build(SplineCurves, LinearApproximation);
		bDirty = false;
	}

	const float TotalSplineAlpha = SplineCurves.ReparamTable.Points.Last().OutVal;
	TwistBlend.SetValueRange(TwistStart, TwistEnd);
	AnimationCore::SolveSplineIK(InputTransforms, SplineCurves.Position, SplineCurves.Rotation, SplineCurves.Scale, TotalSplineAlpha, SplineCurves.GetSplineLength(), FFloatMapping::CreateUObject(this, &USplineIKControlRig::GetTwist, TotalSplineAlpha), Roll, Stretch, Offset, BoneAxis, FFindParamAtFirstSphereIntersection::CreateUObject(this, &USplineIKControlRig::FindParamAtFirstSphereIntersection), CachedOffsetRotations, CachedBoneLengths, OriginalSplineLength, OutputTransforms);
}

float USplineIKControlRig::GetTwist(float Alpha, float TotalSplineAlpha)
{
	TwistBlend.SetAlpha(Alpha / TotalSplineAlpha);
	return TwistBlend.GetBlendedValue();
}

float USplineIKControlRig::FindParamAtFirstSphereIntersection(const FVector& InOrigin, float InRadius, int32& StartingLinearIndex)
{
	const float RadiusSquared = InRadius * InRadius;
	const int32 LinearCount = LinearApproximation.Num() - 1;
	for (int32 LinearIndex = StartingLinearIndex; LinearIndex < LinearCount; ++LinearIndex)
	{
		const FSplinePositionLinearApproximation& LinearPoint = LinearApproximation[LinearIndex];
		const FSplinePositionLinearApproximation& NextLinearPoint = LinearApproximation[LinearIndex + 1];

		const float InnerDistanceSquared = (InOrigin - LinearPoint.Position).SizeSquared();
		const float OuterDistanceSquared = (InOrigin - NextLinearPoint.Position).SizeSquared();
		if (InnerDistanceSquared <= RadiusSquared && OuterDistanceSquared >= RadiusSquared)
		{
			StartingLinearIndex = LinearIndex;

			const float InnerDistance = FMath::Sqrt(InnerDistanceSquared);
			const float OuterDistance = FMath::Sqrt(OuterDistanceSquared);
			const float InterpParam = FMath::Clamp((InRadius - InnerDistance) / (OuterDistance - InnerDistance), 0.0f, 1.0f);

			return FMath::Lerp(LinearPoint.SplineParam, NextLinearPoint.SplineParam, InterpParam);
		}
	}

	StartingLinearIndex = 0;
	return SplineCurves.ReparamTable.Points.Last().OutVal;
}

#undef LOCTEXT_NAMESPACE