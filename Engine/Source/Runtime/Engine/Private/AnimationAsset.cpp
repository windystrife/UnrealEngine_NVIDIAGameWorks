// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimationAsset.h"
#include "Engine/AssetUserData.h"
#include "Animation/AssetMappingTable.h"
#include "Animation/AnimSequence.h"
#include "AnimationUtils.h"
#include "Animation/AnimInstance.h"
#include "UObject/LinkerLoad.h"

#define LEADERSCORE_ALWAYSLEADER  	2.f
#define LEADERSCORE_MONTAGE			3.f

FVector FRootMotionMovementParams::RootMotionScale(1.0f, 1.0f, 1.0f);

//////////////////////////////////////////////////////////////////////////
// FAnimGroupInstance

void FAnimGroupInstance::TestTickRecordForLeadership(EAnimGroupRole::Type MembershipType)
{
	// always set leader score if you have potential to be leader
	// that way if the top leader fails, we'll continue to search next available leader
	int32 TestIndex = ActivePlayers.Num() - 1;
	FAnimTickRecord& Candidate = ActivePlayers[TestIndex];

	switch (MembershipType)
	{
	case EAnimGroupRole::CanBeLeader:
	case EAnimGroupRole::TransitionLeader:
		Candidate.LeaderScore = Candidate.EffectiveBlendWeight;
		break;
	case EAnimGroupRole::AlwaysLeader:
		// Always set the leader index
		Candidate.LeaderScore = LEADERSCORE_ALWAYSLEADER;
		break;
	default:
	case EAnimGroupRole::AlwaysFollower:
	case EAnimGroupRole::TransitionFollower:
		// Never set the leader index; the actual tick code will handle the case of no leader by using the first element in the array
		break;
	}
}

void FAnimGroupInstance::TestMontageTickRecordForLeadership()
{
	int32 TestIndex = ActivePlayers.Num() - 1;
	ensure(TestIndex <= 1);
	FAnimTickRecord& Candidate = ActivePlayers[TestIndex];

	// if the candidate has higher weight
	if (Candidate.EffectiveBlendWeight > MontageLeaderWeight)
	{
		// if this is going to be leader, I'll clean ActivePlayers because we don't sync multi montages
		const int32 LastIndex = TestIndex - 1;
		if (LastIndex >= 0)
		{
			ActivePlayers.RemoveAt(TestIndex - 1, 1);
		}

		// at this time, it should only have one
		ensure(ActivePlayers.Num() == 1);

		// then override
		// @note : leader weight doesn't applied WITHIN montages
		// we still only contain one montage at a time, if this montage fails, next candidate will get the chance, not next weight montage
		MontageLeaderWeight = Candidate.EffectiveBlendWeight;
		Candidate.LeaderScore = LEADERSCORE_MONTAGE;
	}
	else
	{
		if (TestIndex != 0)
		{
			// we delete the later ones because we only have one montage for leader. 
			// this can happen if there was already active one with higher weight. 
			ActivePlayers.RemoveAt(TestIndex, 1);
		}
	}

	ensureAlways(ActivePlayers.Num() == 1);
}

void FAnimGroupInstance::Finalize(const FAnimGroupInstance* PreviousGroup)
{
	if (!PreviousGroup || PreviousGroup->GroupLeaderIndex != GroupLeaderIndex
		|| (PreviousGroup->MontageLeaderWeight > 0.f && MontageLeaderWeight == 0.f/*if montage disappears, we should reset as well*/))
	{
		UE_LOG(LogAnimMarkerSync, Log, TEXT("Resetting Marker Sync Groups"));

		for (int32 RecordIndex = GroupLeaderIndex + 1; RecordIndex < ActivePlayers.Num(); ++RecordIndex)
		{
			ActivePlayers[RecordIndex].MarkerTickRecord->Reset();
		}
	}
}

void FAnimGroupInstance::Prepare(const FAnimGroupInstance* PreviousGroup)
{
	ActivePlayers.Sort();

	TArray<FName>* MarkerNames = ActivePlayers[0].SourceAsset->GetUniqueMarkerNames();
	if (MarkerNames)
	{
		// Group leader has markers, off to a good start
		ValidMarkers = *MarkerNames;
		ActivePlayers[0].bCanUseMarkerSync = true;
		bCanUseMarkerSync = true;

		//filter markers based on what exists in the other animations
		for ( int32 ActivePlayerIndex = 0; ActivePlayerIndex < ActivePlayers.Num(); ++ActivePlayerIndex )
		{
			FAnimTickRecord& Candidate = ActivePlayers[ActivePlayerIndex];

			if (PreviousGroup)
			{
				bool bCandidateFound = false;
				for (const FAnimTickRecord& PrevRecord : PreviousGroup->ActivePlayers)
				{
					if (PrevRecord.MarkerTickRecord == Candidate.MarkerTickRecord)
					{
						// Found previous record for "us"
						if (PrevRecord.SourceAsset != Candidate.SourceAsset)
						{
							Candidate.MarkerTickRecord->Reset(); // Changed animation, clear our cached data
						}
						bCandidateFound = true;
						break;
					}
				}
				if (!bCandidateFound)
				{
					Candidate.MarkerTickRecord->Reset(); // we weren't active last frame, reset
				}
			}

			if (ActivePlayerIndex != 0 && ValidMarkers.Num() > 0)
			{
				TArray<FName>* PlayerMarkerNames = Candidate.SourceAsset->GetUniqueMarkerNames();
				if ( PlayerMarkerNames ) // Let anims with no markers set use length scaling sync
				{
					Candidate.bCanUseMarkerSync = true;
					for ( int32 ValidMarkerIndex = ValidMarkers.Num() - 1; ValidMarkerIndex >= 0; --ValidMarkerIndex )
					{
						FName& MarkerName = ValidMarkers[ValidMarkerIndex];
						if ( !PlayerMarkerNames->Contains(MarkerName) )
						{
							ValidMarkers.RemoveAtSwap(ValidMarkerIndex, 1, false);
						}
					}
				}
			}
		}

		bCanUseMarkerSync = ValidMarkers.Num() > 0;

		ValidMarkers.Sort();

		if (!PreviousGroup || (ValidMarkers != PreviousGroup->ValidMarkers))
		{
			for (int32 InternalActivePlayerIndex = 0; InternalActivePlayerIndex < ActivePlayers.Num(); ++InternalActivePlayerIndex)
			{
				ActivePlayers[InternalActivePlayerIndex].MarkerTickRecord->Reset();
			}
		}
	}
	else
	{
		// Leader has no markers, we can't use SyncMarkers.
		bCanUseMarkerSync = false;
		ValidMarkers.Reset();
		for (FAnimTickRecord& AnimTickRecord : ActivePlayers)
		{
			AnimTickRecord.MarkerTickRecord->Reset();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UAnimationAsset

UAnimationAsset::UAnimationAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimationAsset::PostLoad()
{
	LLM_SCOPE(ELLMTag::Animation);

	Super::PostLoad();

	// Load skeleton, to make sure anything accessing from PostLoad
	// skeleton is ready
	if (Skeleton)
	{
		if (FLinkerLoad* SkeletonLinker = Skeleton->GetLinker())
		{
			SkeletonLinker->Preload(Skeleton);
		}
		Skeleton->ConditionalPostLoad();
	}

	ValidateSkeleton();

	check( Skeleton==NULL || SkeletonGuid.IsValid() );

#if WITH_EDITOR
	UpdateParentAsset();
#endif // WITH_EDITOR
}

void UAnimationAsset::ResetSkeleton(USkeleton* NewSkeleton)
{
// @TODO LH, I'd like this to work outside of editor, but that requires unlocking track names data in game
#if WITH_EDITOR
	Skeleton = NULL;
	ReplaceSkeleton(NewSkeleton);
#endif
}

void UAnimationAsset::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Animation);

	Super::Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_SKELETON_GUID_SERIALIZATION)
	{
		Ar << SkeletonGuid;
	}
}

void UAnimationAsset::AddMetaData(class UAnimMetaData* MetaDataInstance)
{
	MetaData.Add(MetaDataInstance);
}

void UAnimationAsset::RemoveMetaData(class UAnimMetaData* MetaDataInstance)
{
	MetaData.Remove(MetaDataInstance);
}

void UAnimationAsset::RemoveMetaData(const TArray<UAnimMetaData*> MetaDataInstances)
{
	MetaData.RemoveAll(
		[&](UAnimMetaData* MetaDataInstance)
	{
		return MetaDataInstances.Find(MetaDataInstance);
	});
}

void UAnimationAsset::SetSkeleton(USkeleton* NewSkeleton)
{
	if (NewSkeleton && NewSkeleton != Skeleton)
	{
		Skeleton = NewSkeleton;
		SkeletonGuid = NewSkeleton->GetGuid();
	}
}

#if WITH_EDITOR
void UAnimationAsset::RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces)
{
	SetSkeleton(NewSkeleton);
}

bool UAnimationAsset::ReplaceSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces/*=false*/)
{
	// if it's not same 
	if (NewSkeleton != Skeleton)
	{
		// get all sequences that need to change
		TArray<UAnimationAsset*> AnimAssetsToReplace;

		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(this))
		{
			AnimAssetsToReplace.AddUnique(AnimSequence);
		}
		if (GetAllAnimationSequencesReferred(AnimAssetsToReplace))
		{
			//Firstly need to remap
			for (UAnimationAsset* AnimAsset : AnimAssetsToReplace)
			{
				//Make sure animation has finished loading before we start messing with it
				if (FLinkerLoad* AnimLinker = AnimAsset->GetLinker())
				{
					AnimLinker->Preload(AnimAsset);
				}
				AnimAsset->ConditionalPostLoad();

				// these two are different functions for now
				// technically if you have implementation for Remap, it will also set skeleton 
				AnimAsset->RemapTracksToNewSkeleton(NewSkeleton, bConvertSpaces);
			}

			//Second need to process anim sequences themselves. This is done in two stages as additives can rely on other animations.
			for (UAnimationAsset* AnimAsset : AnimAssetsToReplace)
			{
				if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
				{
					// We don't force gen here as that can cause us to constantly generate
					// new anim ddc keys if users never resave anims that need to remap.
					Seq->PostProcessSequence(false);
				}
			}
		}

		RemapTracksToNewSkeleton(NewSkeleton, bConvertSpaces);

		PostEditChange();
		MarkPackageDirty();
		return true;
	}

	return false;
}

bool UAnimationAsset::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationSequences, bool bRecursive /*= true*/) 
{
	//@todo:@fixme: this doens't work for retargeting because postload gets called after duplication, mixing up the mapping table
	// because skeleton changes, for now we don't support retargeting for parent asset, it will disconnect, and just duplicate everything else
// 	if (ParentAsset)
// 	{
// 		ParentAsset->HandleAnimReferenceCollection(AnimationSequences, bRecursive);
// 	}
// 
// 	if (AssetMappingTable)
// 	{
// 		AssetMappingTable->GetAllAnimationSequencesReferred(AnimationSequences, bRecursive);
// 	}

	return (AnimationSequences.Num() > 0);
}

void UAnimationAsset::HandleAnimReferenceCollection(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive)
{
	AnimationAssets.AddUnique(this);
	if (bRecursive)
	{
		// anim sequence still should call this. since bRecursive is true, I'm not sending the parameter with it
		GetAllAnimationSequencesReferred(AnimationAssets);
	}
}

void UAnimationAsset::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	//@todo:@fixme: this doens't work for retargeting because postload gets called after duplication, mixing up the mapping table
	// because skeleton changes, for now we don't support retargeting for parent asset, it will disconnect, and just duplicate everything else
	if (ParentAsset)
	{
		// clear up, so that it doesn't try to use from other asset
		ParentAsset = nullptr;
		AssetMappingTable = nullptr;
	}

// 	if (ParentAsset)
// 	{
// 		// now fix everythign else
// 		UAnimationAsset* const* ReplacementAsset = ReplacementMap.Find(ParentAsset);
// 		if (ReplacementAsset)
// 		{
// 			ParentAsset = *ReplacementAsset;
// 			ParentAsset->ReplaceReferredAnimations(ReplacementMap);
// 		}
// 	}
// 
// 	if (AssetMappingTable)
// 	{
// 		AssetMappingTable->ReplaceReferredAnimations(ReplacementMap);
// 	}
}

USkeletalMesh* UAnimationAsset::GetPreviewMesh()
{
	USkeletalMesh* PreviewMesh = PreviewSkeletalMesh.LoadSynchronous();
	// if somehow skeleton changes, just nullify it. 
	if (PreviewMesh && PreviewMesh->Skeleton != Skeleton)
	{
		PreviewMesh = nullptr;
		SetPreviewMesh(nullptr);
	}

	return PreviewMesh;
}

USkeletalMesh* UAnimationAsset::GetPreviewMesh() const
{
	return PreviewSkeletalMesh.Get();
}

void UAnimationAsset::SetPreviewMesh(USkeletalMesh* PreviewMesh)
{
	Modify();
	PreviewSkeletalMesh = PreviewMesh;
}

void UAnimationAsset::UpdateParentAsset()
{
	ValidateParentAsset();

	if (ParentAsset)
	{
		TArray<UAnimationAsset*> AnimAssetsReferencedDirectly;
		if (ParentAsset->GetAllAnimationSequencesReferred(AnimAssetsReferencedDirectly, false))
		{
			AssetMappingTable->RefreshAssetList(AnimAssetsReferencedDirectly);
		}
	}
	else
	{
		// if somehow source data is gone, there is nothing much to do here
		ParentAsset = nullptr;
		AssetMappingTable = nullptr;
	}

	if (ParentAsset)
	{
		RefreshParentAssetData();
	}
}

void UAnimationAsset::ValidateParentAsset()
{
	if (ParentAsset)
	{
		if (ParentAsset->GetSkeleton() != GetSkeleton())
		{
			// parent asset chnaged skeleton, so we'll have to discard parent asset
			UE_LOG(LogAnimation, Warning, TEXT("%s: ParentAsset %s linked to different skeleton. Removing the reference."), *GetName(), *GetNameSafe(ParentAsset));
			ParentAsset = nullptr;
			Modify();
		}
		else if (ParentAsset->StaticClass() != StaticClass())
		{
			// parent asset chnaged skeleton, so we'll have to discard parent asset
			UE_LOG(LogAnimation, Warning, TEXT("%s: ParentAsset %s class type doesn't match. Removing the reference."), *GetName(), *GetNameSafe(ParentAsset));
			ParentAsset = nullptr;
			Modify();
		}
	}
}

void UAnimationAsset::RefreshParentAssetData()
{
	// should only allow within the same skeleton
	ParentAsset->ChildrenAssets.AddUnique(this);
	MetaData = ParentAsset->MetaData;
	PreviewPoseAsset = ParentAsset->PreviewPoseAsset;
	PreviewSkeletalMesh = ParentAsset->PreviewSkeletalMesh;
}

void UAnimationAsset::SetParentAsset(UAnimationAsset* InParentAsset)
{
	// only same class and same skeleton
	if (InParentAsset && InParentAsset->HasParentAsset() == false && 
		InParentAsset->StaticClass() == StaticClass() && InParentAsset->GetSkeleton() == GetSkeleton())
	{
		ParentAsset = InParentAsset;

		// if ParentAsset exists, just create mapping table.
		// it becomes messy if we only created when we have assets to map
		if (AssetMappingTable == nullptr)
		{
			AssetMappingTable = NewObject<UAssetMappingTable>(this);
		}
		else
		{
			AssetMappingTable->Clear();
		}

		UpdateParentAsset();
	}
	else
	{
		// otherwise, clear it
		ParentAsset = nullptr;
		AssetMappingTable = nullptr;
	}
}

bool UAnimationAsset::RemapAsset(UAnimationAsset* SourceAsset, UAnimationAsset* TargetAsset)
{
	if (AssetMappingTable)
	{
		if (AssetMappingTable->RemapAsset(SourceAsset, TargetAsset))
		{
			RefreshParentAssetData();
			return true;
		}
	}

	return false;
}
#endif

void UAnimationAsset::ValidateSkeleton()
{
	if (Skeleton && Skeleton->GetGuid() != SkeletonGuid)
	{
		// reset Skeleton
		ResetSkeleton(Skeleton);
		UE_LOG(LogAnimation, Verbose, TEXT("Needed to reset skeleton. Resave this asset to speed up load time: %s"), *GetPathNameSafe(this));
	}
}

void UAnimationAsset::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UAnimationAsset::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UAnimationAsset::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UAnimationAsset::GetAssetUserDataArray() const
{
	return &AssetUserData;
}
///////////////////////////////////////////////////////////////////////////////////////
//
// FBlendSampleData
//
////////////////////////////////////////////////////////////////////////////////////////
void FBlendSampleData::NormalizeDataWeight(TArray<FBlendSampleData>& SampleDataList)
{
	float TotalSum = 0.f;

	check(SampleDataList.Num() > 0);
	const int32 NumBones = SampleDataList[0].PerBoneBlendData.Num();

	TArray<float> PerBoneTotalSums;
	PerBoneTotalSums.AddZeroed(NumBones);

	for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
	{
		checkf(SampleDataList[PoseIndex].PerBoneBlendData.Num() == NumBones, TEXT("Attempted to normalise a blend sample list, but the samples have differing numbers of bones."));

		TotalSum += SampleDataList[PoseIndex].GetWeight();

		if (SampleDataList[PoseIndex].PerBoneBlendData.Num() > 0)
		{
			// now interpolate the per bone weights
			for (int32 BoneIndex = 0; BoneIndex<NumBones; BoneIndex++)
			{
				PerBoneTotalSums[BoneIndex] += SampleDataList[PoseIndex].PerBoneBlendData[BoneIndex];
			}
		}
	}

	// Re-normalize Pose weight
	if (ensure(TotalSum > ZERO_ANIMWEIGHT_THRESH))
	{
		if (FMath::Abs<float>(TotalSum - 1.f) > ZERO_ANIMWEIGHT_THRESH)
		{
			for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
			{
				SampleDataList[PoseIndex].TotalWeight /= TotalSum;
			}
		}
	}

	// Re-normalize per bone weights.
	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		if (ensure(PerBoneTotalSums[BoneIndex] > ZERO_ANIMWEIGHT_THRESH))
		{
			if (FMath::Abs<float>(PerBoneTotalSums[BoneIndex] - 1.f) > ZERO_ANIMWEIGHT_THRESH)
			{
				for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
				{
					SampleDataList[PoseIndex].PerBoneBlendData[BoneIndex] /= PerBoneTotalSums[BoneIndex];
				}
			}
		}
	}
}

