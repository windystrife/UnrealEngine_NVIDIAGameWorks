// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "BoneMappingHelper.h"
#include "AnimationRuntime.h"

//////////////////////////////////////////////////////////////////////////
// FBoneDescription
//////////////////////////////////////////////////////////////////////////

int32 FBoneDescription::GetBestIndex() const
{
	int32 BestIndex = INDEX_NONE;
	float BestScore = 0.f;

	for (int32 Index = 0; Index < Scores.Num(); ++Index)
	{
		if (Scores[Index] > BestScore)
		{
			BestScore = Scores[Index];
			BestIndex = Index;
		}
	}

	return BestIndex;
}

float FBoneDescription::CalculateNameScore(const FName& Name1, const FName& Name2) const
{
	FString String1 = Name1.ToString();
	FString String2 = Name2.ToString();
	int32 Distance = FAnimationRuntime::GetStringDistance(String1, String2);
	float MaxLength = FMath::Max(String1.Len() , String2.Len());
	return (MaxLength) ? (MaxLength - Distance) / (MaxLength) : 1.f;
}

float FBoneDescription::CalculateScore(const FBoneDescription& Other) const
{
	// if they don't have parent, it's root, so just give whole score
	if (Other.BoneInfo.ParentIndex == INDEX_NONE && BoneInfo.ParentIndex == INDEX_NONE)
	{
		return 1.f;
	}

	// calculating score is tricky
	float Score = 0.f;

	// each element will exit from [0, 1], and we'll apply weight to it

	// check direction of facing [-1, 1]
	float Cosine = FVector::DotProduct(DirFromParent, Other.DirFromParent);
	float Score_DirFromParent = FMath::Max((Cosine-0.5f)*2.f, 0.f); // scale to only care for range of [0.5-1]
	Score_DirFromParent = FMath::Clamp(Score_DirFromParent, 0.f, 1.f);

	// check direction of facing [-1, 1]
	Cosine = FVector::DotProduct(DirFromRoot, Other.DirFromRoot);
	float Score_DirFromRoot = FMath::Max((Cosine - 0.5f)*2.f, 0.f); // scale to only care for range of [0.5-1]
	Score_DirFromRoot = FMath::Clamp(Score_DirFromRoot, 0.f, 1.f);

	// check number of children
	float Score_NumChildren = 0.f;
	const int32 MaxNumChildren = FMath::Max(Other.NumChildren, NumChildren);
	if (MaxNumChildren > 0)
	{
		Score_NumChildren = (1.f - ((float)(FMath::Abs(Other.NumChildren - NumChildren)) / MaxNumChildren));
	}
	else
	{
		// no children - leaf node
		Score_NumChildren = 1.f;
	}

	Score_NumChildren = FMath::Clamp(Score_NumChildren, 0.f, 1.f);

	// score of ratio from parent  - if you're here, you should have parent, so it shouldn't be 0.f, most likely
	float Score_RatioFromParent = 0.f;
	if (Other.RatioFromParent > RatioFromParent)
	{
		Score_RatioFromParent = (Other.RatioFromParent > 0.f) ? RatioFromParent / Other.RatioFromParent : 0.f;
	}
	else
	{
		Score_RatioFromParent = (RatioFromParent > 0.f) ? Other.RatioFromParent / RatioFromParent : 0.f;
	}

	Score_RatioFromParent = FMath::Clamp(Score_RatioFromParent, 0.f, 1.f);

	// check normalized position - since this is normalized, it should stay within 1
	FVector DiffNormalizedPosition = (Other.NormalizedPosition - NormalizedPosition).GetAbs();
	const float MaxNormalizedPosition = 3.f; /* since 1^2+1^2+1^2 = 3*/
	float Score_NormalizedPosition = (MaxNormalizedPosition - DiffNormalizedPosition.SizeSquared())/ MaxNormalizedPosition;
	Score_NormalizedPosition = FMath::Clamp(Score_NormalizedPosition, 0.f, 1.f);

	float Score_NameMatching = CalculateNameScore(BoneInfo.Name, Other.BoneInfo.Name);
	Score_NameMatching = FMath::Clamp(Score_NameMatching, 0.f, 1.f);

	// now come up with full score
	static float Weight_DirFromParent = 2.f;
	static float Weight_NumChildren = 0.5f;
	static float Weight_NormalizedPosition = 1.0f; // location can be very confusing, so give less weight on this
	static float Weight_RatioFromParent = 1.f;
	static float Weight_NameMatching = 2.0f;
	static float Weight_DirFromRoot = 0.f;

	float FinalScore = (Score_DirFromParent * Weight_DirFromParent + Score_NumChildren * Weight_NumChildren + Score_NormalizedPosition * Weight_NormalizedPosition + Score_RatioFromParent * Weight_RatioFromParent + Score_NameMatching * Weight_NameMatching + Score_DirFromRoot * Weight_DirFromRoot) / (Weight_DirFromParent + Weight_NumChildren + Weight_NormalizedPosition + Weight_RatioFromParent + Weight_NameMatching + Weight_DirFromRoot);
	UE_LOG(LogAnimation, Log, TEXT("Calculate Score - [%s] - [%s] (Score_DirFromParent(%0.2f), Score_NumChildren(%0.2f), Score_NormalizedPosition(%0.2f), Score_RatioFromParent(%0.2f), Score_NameMatching(%0.2f), Score_DirFromRoot(%0.2f) )"), *BoneInfo.Name.ToString(), *Other.BoneInfo.Name.ToString(), Score_DirFromParent, Score_NumChildren, Score_NormalizedPosition, Score_RatioFromParent, Score_NameMatching, Score_DirFromRoot);
	return FinalScore;
}

//////////////////////////////////////////////////////////////////////////
// FBoneMappingHelper
//////////////////////////////////////////////////////////////////////////

FBoneMappingHelper::FBoneMappingHelper(const FReferenceSkeleton& InRefSkeleton1, const FReferenceSkeleton&  InRefSkeleton2)
{
	Initialize(0, InRefSkeleton1);
	Initialize(1, InRefSkeleton2);
}

void FBoneMappingHelper::Initialize(int32 Index, const FReferenceSkeleton&  InRefSkeleton)
{
	int32 TotalNum = InRefSkeleton.GetNum();
	TArray<FBoneDescription>& BoneDescList = BoneDescs[Index];

	BoneDescList.Reset(TotalNum);
	RefSkeleton[Index] = InRefSkeleton;

	if (TotalNum > 0)
	{
		BoneDescList.AddZeroed(TotalNum);

		// fill up component space transforms
		TArray<FTransform> ComponentSpaceTransforms;
		FAnimationRuntime::FillUpComponentSpaceTransforms(InRefSkeleton, InRefSkeleton.GetRefBonePose(), ComponentSpaceTransforms);

		// get extent
		FBox BoxExtent;
		BoxExtent.Init();

		for (int32 BoneIndex = 0; BoneIndex < TotalNum; ++BoneIndex)
		{
			BoxExtent += ComponentSpaceTransforms[BoneIndex].GetLocation();
		}

		const FVector MeshBoxSize = BoxExtent.GetSize();
		const FVector RootPosition = ComponentSpaceTransforms[0].GetTranslation();

		if (MeshBoxSize.GetMin() > SMALL_NUMBER)
		{
			// now fill up data
			const TArray<FMeshBoneInfo>& MeshInfoList = InRefSkeleton.GetRefBoneInfo();
			for (int32 BoneIndex = 0; BoneIndex < TotalNum; ++BoneIndex)
			{
				FBoneDescription& BoneDesc = BoneDescList[BoneIndex];
				BoneDesc.BoneInfo = MeshInfoList[BoneIndex];
				BoneDesc.NumChildren = 0;

				// normalized position
				const FVector& Position = ComponentSpaceTransforms[BoneIndex].GetLocation();
				BoneDesc.NormalizedPosition = (Position - BoxExtent.Min) / MeshBoxSize;

				// increase parent child
				if (BoneDesc.BoneInfo.ParentIndex != INDEX_NONE)
				{
					const int32 ParentIndex = BoneDesc.BoneInfo.ParentIndex;
					++BoneDescList[ParentIndex].NumChildren;

					const FVector& ParentPosition = ComponentSpaceTransforms[ParentIndex].GetLocation();
					FVector ToChild = (Position-ParentPosition);
					BoneDesc.RatioFromParent = ToChild.Size() / MeshBoxSize.Size();
					BoneDesc.DirFromParent = ToChild.GetSafeNormal();
					BoneDesc.DirFromRoot = (Position-RootPosition).GetSafeNormal(); // based on whole mesh size
				}
			}
		}
	}
}

void FBoneMappingHelper::TryMatch(TMap<FName, FName>& OutBestMatches)
{
	TArray<FBoneDescription>& BoneDescArray0 = BoneDescs[0];
	TArray<FBoneDescription>& BoneDescArray1 = BoneDescs[1];

	for (int32 BoneIndex0 = 0; BoneIndex0 < BoneDescArray0.Num(); ++BoneIndex0)
	{
		BoneDescArray0[BoneIndex0].ResetScore(BoneDescArray1.Num());
	}

	for (int32 BoneIndex1 = 0; BoneIndex1 < BoneDescArray1.Num(); ++BoneIndex1)
	{
		BoneDescArray1[BoneIndex1].ResetScore(BoneDescArray0.Num());
	}

	for (int32 BoneIndex0 = 0; BoneIndex0 < BoneDescArray0.Num(); ++BoneIndex0)
	{
		FBoneDescription& BoneDesc0 = BoneDescArray0[BoneIndex0];
		for (int32 BoneIndex1 = 0; BoneIndex1 < BoneDescArray1.Num(); ++BoneIndex1)
		{
			FBoneDescription& BoneDesc1 = BoneDescArray1[BoneIndex1];

			// set score in both container
			float Score = BoneDesc0.CalculateScore(BoneDesc1);
			BoneDesc0.SetScore(BoneIndex1, Score);
			BoneDesc1.SetScore(BoneIndex0, Score);
		}
	}

#define MAX_CANDIDATE 10

	// first find best matches up to MAX_CANDIDATE for each
	struct FCandidate
	{
		FName BoneNames[MAX_CANDIDATE];
		float Scores[MAX_CANDIDATE];
		float StdDev;

		FCandidate()
		{
			FMemory::Memzero(Scores);
			StdDev = 0.f;
		}

		void Set(FName InBoneName, float InScore, int32 Index)
		{
			BoneNames[Index] = InBoneName;
			Scores[Index] = InScore;
		}

		void CalculateStdDev()
		{
			float Avg = 0.f;

			for (int32 Index = 0; Index < MAX_CANDIDATE; ++Index)
			{
				Avg += Scores[Index];
			}

			Avg /= MAX_CANDIDATE;
			
			float AccumulatedDev = 0.f;

			for (int32 Index = 0; Index < MAX_CANDIDATE; ++Index)
			{
				AccumulatedDev += FMath::Square((Scores[Index]) - Avg);
			}

			StdDev = FGenericPlatformMath::Sqrt(AccumulatedDev/MAX_CANDIDATE);
		}
	};

	TMap<FName, FCandidate> Candidates;

	// find the best score
	for (int32 BoneIndex0 = 0; BoneIndex0 < BoneDescArray0.Num(); ++BoneIndex0)
	{
		int32 BoneIndex1 = BoneDescArray0[BoneIndex0].GetBestIndex();

		if (BoneIndex1 != INDEX_NONE)
		{
			FCandidate Candidate;
			FName Bone0Name = BoneDescArray0[BoneIndex0].BoneInfo.Name;
			FName Bone1Name = BoneDescArray1[BoneIndex1].BoneInfo.Name;
			float Score = BoneDescArray0[BoneIndex0].GetScore(BoneIndex1);
			Candidate.Set(Bone1Name, Score, 0);

			UE_LOG(LogAnimation, Log, TEXT("Bone Match [%s] - [%s] (score %0.2f)"), *Bone0Name.ToString(), *Bone1Name.ToString(), Score);

			// print next 3 best score, clear current one
			for (int32 LoopCount = 0; LoopCount < MAX_CANDIDATE-1 && BoneIndex1 != INDEX_NONE; ++LoopCount)
			{
				BoneDescArray0[BoneIndex0].SetScore(BoneIndex1, 0.f);

				BoneIndex1 = BoneDescArray0[BoneIndex0].GetBestIndex();
				if (BoneIndex1 != INDEX_NONE)
				{
					Bone1Name = BoneDescArray1[BoneIndex1].BoneInfo.Name;
					Score = BoneDescArray0[BoneIndex0].GetScore(BoneIndex1);

					Candidate.Set(Bone1Name, Score, LoopCount + 1);
					UE_LOG(LogAnimation, Log, TEXT(" Candidate %d. - [%s] (score %0.2f)"), LoopCount + 1, *Bone1Name.ToString(), Score);
				}
			}

			Candidate.CalculateStdDev();
			Candidates.Add(Bone0Name, Candidate);
		}
		else
		{
			UE_LOG(LogAnimation, Log, TEXT("Bone [%s] does not have a match"), *BoneDescArray0[BoneIndex0].BoneInfo.Name.ToString());
		}
	}

	struct FCandidateSortCallback
	{
		FORCEINLINE bool operator()(const FCandidate& A, const FCandidate& B) const
		{
			return A.StdDev > B.StdDev;
		}
	};

	Candidates.ValueSort(FCandidateSortCallback());

	TArray<FName> UsedNames;
	for (const TPair<FName, FCandidate>& CandidatePair : Candidates)
	{
		const FCandidate& Candidate = CandidatePair.Value;
		for (int32 Index = 0; Index < MAX_CANDIDATE; ++Index)
		{
			FName BestMatchName = Candidate.BoneNames[Index];
			// see if it's already used by other joint
			if (UsedNames.Find(BestMatchName) == INDEX_NONE)
			{
				// if that's case, just ignore and move on
				OutBestMatches.Add(CandidatePair.Key, BestMatchName);
				UsedNames.Add(BestMatchName);
				break;
			}
		}
	}
}

