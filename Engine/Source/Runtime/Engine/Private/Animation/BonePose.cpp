// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BonePose.h"
#include "AnimationRuntime.h"

void FMeshPose::ResetToRefPose()
{
	FAnimationRuntime::FillWithRefPose(Bones, *BoneContainer);
}

void FMeshPose::ResetToIdentity()
{
	FAnimationRuntime::InitializeTransform(*BoneContainer, Bones);
}


bool FMeshPose::ContainsNaN() const
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = BoneContainer->GetBoneIndicesArray();
	for (int32 Iter = 0; Iter < RequiredBoneIndices.Num(); ++Iter)
	{
		const int32 BoneIndex = RequiredBoneIndices[Iter];
		if (Bones[BoneIndex].ContainsNaN())
		{
			return true;
		}
	}

	return false;
}

bool FMeshPose::IsNormalized() const
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = BoneContainer->GetBoneIndicesArray();
	for (int32 Iter = 0; Iter < RequiredBoneIndices.Num(); ++Iter)
	{
		int32 BoneIndex = RequiredBoneIndices[Iter];
		const FTransform& Trans = Bones[BoneIndex];
		if (!Bones[BoneIndex].IsRotationNormalized())
		{
			return false;
		}
	}

	return true;
}

