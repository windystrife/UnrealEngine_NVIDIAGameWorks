// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseSnapshot.generated.h"

/** A pose for a skeletal mesh */
USTRUCT(BlueprintType)
struct ENGINE_API FPoseSnapshot
{
	GENERATED_BODY()

public:
	/** Array of transforms per-bone */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Snapshot")
	TArray<FTransform> LocalTransforms;

	/** Array of bone names (corresponding to LocalTransforms) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Snapshot")
	TArray<FName> BoneNames;

	/** The name of the skeletal mesh that was used to take this snapshot */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Snapshot")
	FName SkeletalMeshName;

	/** The name for this snapshot*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Snapshot")
	FName SnapshotName;

	/** Whether the pose is valid */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Snapshot")
	bool bIsValid;
};
