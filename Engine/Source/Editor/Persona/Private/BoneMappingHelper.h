// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReferenceSkeleton.h"

//////////////////////////////////////////////////////////////////////////
// FBoneDescription
//////////////////////////////////////////////////////////////////////////
struct FBoneDescription
{
	FMeshBoneInfo BoneInfo;

	FVector NormalizedPosition; // based on whole mesh size
	FVector DirFromParent;
	FVector DirFromRoot;
	float	RatioFromParent; // based on whole mesh size
	int32	NumChildren;

	TArray<float> Scores;

	void ResetScore(int32 NewCount)
	{
		Scores.Reset(NewCount);
		Scores.AddZeroed(NewCount);
	}

	void SetScore(int32 Index, float NewScore)
	{
		Scores[Index] = NewScore;
	}

	float GetScore(int32 Index) const
	{
		return Scores[Index];
	}

	float CalculateScore(const FBoneDescription& Other) const;
	float CalculateNameScore(const FName& Name1, const FName& Name2) const;
	int32 GetBestIndex() const;
};

//////////////////////////////////////////////////////////////////////////
// BoneMappingHelper Class

struct FBoneMappingHelper
{
	// input - 2 skeletons
	FReferenceSkeleton RefSkeleton[2];

	// initialize data
	FBoneMappingHelper(const FReferenceSkeleton& InRefSkeleton1, const FReferenceSkeleton&  InRefSkeleton2);

	void TryMatch(TMap<FName, FName>& OutBestMatches);

private:
	// BoneDescription array for each bone
	TArray<FBoneDescription>	BoneDescs[2];

	void Initialize(int32 Index, const FReferenceSkeleton&  InRefSkeleton);
};
