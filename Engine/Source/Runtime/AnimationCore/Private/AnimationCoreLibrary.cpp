// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationCore.h: Render core module implementation.
=============================================================================*/

#include "AnimationCoreLibrary.h"
#include "Constraint.h"
#include "AnimationCoreUtil.h"

namespace AnimationCore
{

static void AccumulateConstraintTransform(const FTransform& TargetTransform, const FConstraintDescription& Operator, float Weight, FComponentBlendHelper& BlendHelper)
{
	// now filter by operation
	if (Operator.bParent)
	{
		BlendHelper.AddParent(TargetTransform, Weight);
	}
	else
	{
		if (Operator.bTranslation)
		{
			FVector4 Translation = TargetTransform.GetTranslation();
			Operator.TranslationAxes.FilterVector(Translation);
			BlendHelper.AddTranslation(FVector(Translation.X, Translation.Y, Translation.Z), Weight);
		}

		if (Operator.bRotation)
		{
			FQuat DeltaRotation = TargetTransform.GetRotation();
			FVector4 DeltaRotationVector(DeltaRotation.X, DeltaRotation.Y, DeltaRotation.Z, DeltaRotation.W);
			Operator.RotationAxes.FilterVector(DeltaRotationVector);
			DeltaRotation = FQuat(DeltaRotationVector.X, DeltaRotationVector.Y, DeltaRotationVector.Z, DeltaRotationVector.W);
			DeltaRotation.Normalize();
			BlendHelper.AddRotation(DeltaRotation, Weight);
		}

		if (Operator.bScale)
		{
			FVector4 Scale = TargetTransform.GetScale3D();
			Operator.ScaleAxes.FilterVector(Scale);
			BlendHelper.AddScale(FVector(Scale.X, Scale.Y, Scale.Z), Weight);
		}
	}
}

FTransform SolveConstraints(const FTransform& CurrentTransform, const FTransform& BaseTransform, const TArray<struct FTransformConstraint>& Constraints, const FGetGlobalTransform& OnGetGlobalTransform)
{
	int32 TotalNum = Constraints.Num();
	ensureAlways(TotalNum > 0);
	check(OnGetGlobalTransform.IsBound());

	FComponentBlendHelper BlendHelper;
	FTransform BlendedTransform = CurrentTransform;

	float TotalWeight = 0.f;
	for (int32 ConstraintIndex = 0; ConstraintIndex < TotalNum; ++ConstraintIndex)
	{
		const FTransformConstraint& Constraint = Constraints[ConstraintIndex];

		if (Constraint.Weight > ZERO_ANIMWEIGHT_THRESH)
		{
			// constraint has to happen in relative to parent to keep the hierarchy data
			// we'd like to test if this would work well with rotation
			FTransform ConstraintTransform = OnGetGlobalTransform.Execute(Constraint.TargetNode);
			FTransform ConstraintToParent = ConstraintTransform.GetRelativeTransform(BaseTransform);
			AccumulateConstraintTransform(ConstraintToParent, Constraint.Operator, Constraint.Weight, BlendHelper);
		}
	}

	// @note : parent and any other combination of constraints won't work
	FTransform ParentTransform;
	if (BlendHelper.GetBlendedParent(ParentTransform))
	{
		BlendedTransform = ParentTransform;
	}
	else
	{
		FVector BlendedTranslation;
		if (BlendHelper.GetBlendedTranslation(BlendedTranslation))
		{
			// if any result
			BlendedTransform.SetTranslation(BlendedTranslation);
		}
		FQuat BlendedRotation;
		if (BlendHelper.GetBlendedRotation(BlendedRotation))
		{
			BlendedTransform.SetRotation(BlendedRotation);
		}
		FVector BlendedScale;
		if (BlendHelper.GetBlendedScale(BlendedScale))
		{
			BlendedTransform.SetScale3D(BlendedScale);
		}
	}

	return BlendedTransform;
}

FQuat SolveAim(const FTransform& CurrentTransform, const FVector& TargetPosition, const FVector& AimVector, bool bUseUpVector /*= false*/, const FVector& UpVector /*= FVector::UpVector*/, float AimClampInDegree /*= 0.f*/)
{
	if (!ensureAlways(AimVector.IsNormalized()) || !ensureAlways(!bUseUpVector || UpVector.IsNormalized()))
	{
		return FQuat::Identity;
	}

	FTransform NewTransform = CurrentTransform;
	FVector ToTarget = TargetPosition - NewTransform.GetLocation();
	ToTarget.Normalize();

	if (AimClampInDegree > ZERO_ANIMWEIGHT_THRESH)
	{
		float AimClampInRadians = FMath::DegreesToRadians(FMath::Min(AimClampInDegree, 180.f));
		float DiffAngle = FMath::Acos(FVector::DotProduct(AimVector, ToTarget));

		if (DiffAngle > AimClampInRadians)
		{
			check(DiffAngle > 0.f);

			FVector DeltaTarget = ToTarget - AimVector;
			// clamp delta target to within the ratio
			DeltaTarget *= (AimClampInRadians / DiffAngle);
			// set new target
			ToTarget = AimVector + DeltaTarget;
			ToTarget.Normalize();
		}
	}

	// if want to use look up, project to the plane
	if (bUseUpVector)
	{
		// project target to the plane
		ToTarget = FVector::VectorPlaneProject(ToTarget, UpVector);
		ToTarget.Normalize();
	}

	return FQuat::FindBetweenNormals(AimVector, ToTarget);
}

///////////////////////////////////////////////////////////////
// new constraints

FTransform SolveConstraints(const FTransform& CurrentTransform, const FTransform& CurrentParentTransform, const TArray<struct FConstraintData>& Constraints)
{
	int32 TotalNum = Constraints.Num();
	ensureAlways(TotalNum > 0);

	FMultiTransformBlendHelper BlendHelperInLocalSpace;
	FTransform BlendedLocalTransform = CurrentTransform.GetRelativeTransform(CurrentParentTransform);

	float TotalWeight = 0.f;
	for (int32 ConstraintIndex = 0; ConstraintIndex < TotalNum; ++ConstraintIndex)
	{
		const FConstraintData& Constraint = Constraints[ConstraintIndex];

		if (Constraint.Weight > ZERO_ANIMWEIGHT_THRESH)
		{
			// constraint has to happen in relative to parent to keep the hierarchy data
			// we'd like to test if this would work well with rotation
			FTransform ConstraintTransform = Constraint.CurrentTransform;
			Constraint.ApplyConstraintTransform(ConstraintTransform, CurrentTransform, CurrentParentTransform, BlendHelperInLocalSpace);
		}
	}

	// @note : parent and any other combination of constraints won't work
	FTransform ParentTransform;
	if (BlendHelperInLocalSpace.GetBlendedParent(ParentTransform))
	{
		BlendedLocalTransform = ParentTransform;
	}
	else
	{
		FVector BlendedTranslation;
		if (BlendHelperInLocalSpace.GetBlendedTranslation(BlendedTranslation))
		{
			// if any result
			BlendedLocalTransform.SetTranslation(BlendedTranslation);
		}
		FQuat BlendedRotation;
		if (BlendHelperInLocalSpace.GetBlendedRotation(BlendedRotation))
		{
			BlendedLocalTransform.SetRotation(BlendedRotation);
		}
		FVector BlendedScale;
		if (BlendHelperInLocalSpace.GetBlendedScale(BlendedScale))
		{
			BlendedLocalTransform.SetScale3D(BlendedScale);
		}
	}

	return BlendedLocalTransform * CurrentParentTransform;
}
}