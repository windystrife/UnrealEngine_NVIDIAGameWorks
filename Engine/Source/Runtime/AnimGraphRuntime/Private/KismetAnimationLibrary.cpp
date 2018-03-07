// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KismetAnimationLibrary.h"
#include "AnimationCoreLibrary.h"
#include "AnimationCoreLibrary.h"
#include "Blueprint/BlueprintSupport.h"
#include "TwoBoneIK.h"

#define LOCTEXT_NAMESPACE "UKismetAnimationLibrary"

//////////////////////////////////////////////////////////////////////////
// UKismetAnimationLibrary

const FName AnimationLibraryWarning = FName("Animation Library");

UKismetAnimationLibrary::UKismetAnimationLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration(
			AnimationLibraryWarning,
			LOCTEXT("AnimationLibraryWarning", "Animation Library Warning")
		)
	);
}

void UKismetAnimationLibrary::K2_TwoBoneIK(const FVector& RootPos, const FVector& JointPos, const FVector& EndPos, const FVector& JointTarget, const FVector& Effector, FVector& OutJointPos, FVector& OutEndPos, bool bAllowStretching, float StartStretchRatio, float MaxStretchScale)
{
	AnimationCore::SolveTwoBoneIK(RootPos, JointPos, EndPos, JointTarget, Effector, OutJointPos, OutEndPos, bAllowStretching, StartStretchRatio, MaxStretchScale);
}

FTransform UKismetAnimationLibrary::K2_LookAt(const FTransform& CurrentTransform, const FVector& TargetPosition, FVector AimVector, bool bUseUpVector, FVector UpVector, float ClampConeInDegree)
{
	if (AimVector.IsNearlyZero())
	{
		// aim vector should be normalized
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("AimVector should not be zero. Please specify which direction.")), ELogVerbosity::Warning, AnimationLibraryWarning);
		return FTransform::Identity;
	}

	if (bUseUpVector && UpVector.IsNearlyZero())
	{
		// upvector has to be normalized
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("LookUpVector should not be zero. Please specify which direction.")), ELogVerbosity::Warning, AnimationLibraryWarning);
		bUseUpVector = false;
	}

	if (ClampConeInDegree < 0.f || ClampConeInDegree > 180.f)
	{
		// ClampCone is out of range, it will be clamped to (0.f, 180.f)
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("ClampConeInDegree should range from (0, 180). ")), ELogVerbosity::Warning, AnimationLibraryWarning);
	}

	FQuat DiffRotation = AnimationCore::SolveAim(CurrentTransform, TargetPosition, AimVector.GetSafeNormal(), bUseUpVector, UpVector.GetSafeNormal(), ClampConeInDegree);
	FTransform NewTransform = CurrentTransform;
	NewTransform.SetRotation(DiffRotation);
	return NewTransform;
}

#undef LOCTEXT_NAMESPACE
