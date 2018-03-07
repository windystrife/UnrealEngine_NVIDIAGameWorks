// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// #include "UObject/ObjectMacros.h"
// #include "UObject/Object.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetAnimationLibrary.generated.h"

UCLASS()
class ANIMGRAPHRUNTIME_API UKismetAnimationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Two Bone IK Function", bAllowStretching = "false", StartStretchRatio = "1.0", MaxStretchScale = "1.2"))
	static void K2_TwoBoneIK(const FVector& RootPos, const FVector& JointPos, const FVector& EndPos, const FVector& JointTarget, const FVector& Effector, FVector& OutJointPos, FVector& OutEndPos, bool bAllowStretching, float StartStretchRatio, float MaxStretchScale);

	UFUNCTION(BlueprintPure, Category = "Utilities|Animation", meta = (DisplayName = "Look At Function", bUseUpVector = "false"))
	static FTransform K2_LookAt(const FTransform& CurrentTransform, const FVector& TargetPosition, FVector LookAtVector, bool bUseUpVector, FVector UpVector, float ClampConeInDegree);
};

