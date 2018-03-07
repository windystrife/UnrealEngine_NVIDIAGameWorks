// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ReferenceSkeleton.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

FReferenceSkeletonModifier::~FReferenceSkeletonModifier()
{
	RefSkeleton.RebuildRefSkeleton(Skeleton, true);
}

void FReferenceSkeletonModifier::UpdateRefPoseTransform(const int32 BoneIndex, const FTransform& BonePose)
{
	RefSkeleton.UpdateRefPoseTransform(BoneIndex, BonePose);
}

void FReferenceSkeletonModifier::Add(const FMeshBoneInfo& BoneInfo, const FTransform& BonePose)
{
	RefSkeleton.Add(BoneInfo, BonePose);
}

int32 FReferenceSkeletonModifier::FindBoneIndex(const FName& BoneName) const
{
	return RefSkeleton.FindRawBoneIndex(BoneName);
}

const TArray<FMeshBoneInfo>& FReferenceSkeletonModifier::GetRefBoneInfo() const
{
	return RefSkeleton.GetRawRefBoneInfo();
}

FArchive &operator<<(FArchive& Ar, FMeshBoneInfo& F)
{
	Ar << F.Name << F.ParentIndex;

	if (Ar.IsLoading() && (Ar.UE4Ver() < VER_UE4_REFERENCE_SKELETON_REFACTOR))
	{
		FColor DummyColor = FColor::White;
		Ar << DummyColor;
	}

#if WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() >= VER_UE4_STORE_BONE_EXPORT_NAMES)
	{
		if (!Ar.IsCooking() && !Ar.IsFilterEditorOnly())
		{
			Ar << F.ExportName;
		}
	}
	else
	{
		F.ExportName = F.Name.ToString();
	}
#endif

	return Ar;
}


//////////////////////////////////////////////////////////////////////////

FTransform GetComponentSpaceTransform(TArray<uint8>& ComponentSpaceFlags, TArray<FTransform>& ComponentSpaceTransforms, FReferenceSkeleton& RefSkeleton, int32 TargetIndex)
{
	FTransform& This = ComponentSpaceTransforms[TargetIndex];

	if (!ComponentSpaceFlags[TargetIndex])
	{
		const int32 ParentIndex = RefSkeleton.GetParentIndex(TargetIndex);
		This *= GetComponentSpaceTransform(ComponentSpaceFlags, ComponentSpaceTransforms, RefSkeleton, ParentIndex);
		ComponentSpaceFlags[TargetIndex] = 1;
	}
	return This;
}

int32 FReferenceSkeleton::GetRawSourceBoneIndex(const USkeleton* Skeleton, const FName& SourceBoneName) const
{
	for (const FVirtualBone& VB : Skeleton->GetVirtualBones())
	{
		//Is our source another virtual bone
		if (VB.VirtualBoneName == SourceBoneName)
		{
			//return our source virtual bones target, it is the same transform
			//but it exists in the raw bone array
			return FindBoneIndex(VB.TargetBoneName);
		}
	}
	return FindBoneIndex(SourceBoneName);
}

void FReferenceSkeleton::RebuildRefSkeleton(const USkeleton* Skeleton, bool bRebuildNameMap)
{
	if (bRebuildNameMap)
	{
		//On loading FinalRefBone data wont exist but NameToIndexMap will and will be valid
		RebuildNameToIndexMap();
	}

	const int32 NumVirtualBones = Skeleton ? Skeleton->GetVirtualBones().Num() : 0;
	FinalRefBoneInfo = TArray<FMeshBoneInfo>(RawRefBoneInfo, NumVirtualBones);
	FinalRefBonePose = TArray<FTransform>(RawRefBonePose, NumVirtualBones);
	FinalNameToIndexMap = RawNameToIndexMap;

	RequiredVirtualBones.Reset(NumVirtualBones);
	UsedVirtualBoneData.Reset(NumVirtualBones);

	if (NumVirtualBones > 0)
	{
		TArray<uint8> ComponentSpaceFlags;
		ComponentSpaceFlags.AddZeroed(RawRefBonePose.Num());
		ComponentSpaceFlags[0] = 1;

		TArray<FTransform> ComponentSpaceTransforms = TArray<FTransform>(RawRefBonePose);

		for (int32 VirtualBoneIdx = 0; VirtualBoneIdx < NumVirtualBones; ++VirtualBoneIdx)
		{
			const int32 ActualIndex = VirtualBoneIdx + RawRefBoneInfo.Num();
			const FVirtualBone& VB = Skeleton->GetVirtualBones()[VirtualBoneIdx];

			const int32 SourceIndex = GetRawSourceBoneIndex(Skeleton, VB.SourceBoneName);
			const int32 ParentIndex = FindBoneIndex(VB.SourceBoneName);
			const int32 TargetIndex = FindBoneIndex(VB.TargetBoneName);
			if(ParentIndex != INDEX_NONE && TargetIndex != INDEX_NONE)
			{
				FinalRefBoneInfo.Add(FMeshBoneInfo(VB.VirtualBoneName, VB.VirtualBoneName.ToString(), ParentIndex));

				const FTransform TargetCS = GetComponentSpaceTransform(ComponentSpaceFlags, ComponentSpaceTransforms, *this, TargetIndex);
				const FTransform SourceCS = GetComponentSpaceTransform(ComponentSpaceFlags, ComponentSpaceTransforms, *this, SourceIndex);

				FTransform VBTransform = TargetCS.GetRelativeTransform(SourceCS);

				const int32 NewBoneIndex = FinalRefBonePose.Add(VBTransform);
				FinalNameToIndexMap.Add(VB.VirtualBoneName) = NewBoneIndex;
				RequiredVirtualBones.Add(NewBoneIndex);
				UsedVirtualBoneData.Add(FVirtualBoneRefData(NewBoneIndex, SourceIndex, TargetIndex));
			}
		}
	}
}

void FReferenceSkeleton::RemoveDuplicateBones(const UObject* Requester, TArray<FBoneIndexType> & DuplicateBones)
{
	//Process raw bone data only
	const int32 NumBones = RawRefBoneInfo.Num();
	DuplicateBones.Empty();

	TMap<FName, int32> BoneNameCheck;
	bool bRemovedBones = false;
	for (int32 BoneIndex = NumBones - 1; BoneIndex >= 0; BoneIndex--)
	{
		const FName& BoneName = GetBoneName(BoneIndex);
		const int32* FoundBoneIndexPtr = BoneNameCheck.Find(BoneName);

		// Not a duplicate bone, track it.
		if (FoundBoneIndexPtr == NULL)
		{
			BoneNameCheck.Add(BoneName, BoneIndex);
		}
		else
		{
			const int32 DuplicateBoneIndex = *FoundBoneIndexPtr;
			DuplicateBones.Add(DuplicateBoneIndex);

			UE_LOG(LogAnimation, Warning, TEXT("RemoveDuplicateBones: duplicate bone name (%s) detected for (%s)! Indices: %d and %d. Removing the latter."),
				*BoneName.ToString(), *GetNameSafe(Requester), DuplicateBoneIndex, BoneIndex);

			// Remove duplicate bone index, which was added later as a mistake.
			RawRefBonePose.RemoveAt(DuplicateBoneIndex, 1);
			RawRefBoneInfo.RemoveAt(DuplicateBoneIndex, 1);

			// Now we need to fix all the parent indices that pointed to bones after this in the array
			// These must be after this point in the array.
			for (int32 j = DuplicateBoneIndex; j < GetRawBoneNum(); j++)
			{
				if (GetParentIndex(j) >= DuplicateBoneIndex)
				{
					RawRefBoneInfo[j].ParentIndex -= 1;
				}
			}

			// Update entry in case problem bones were added multiple times.
			BoneNameCheck.Add(BoneName, BoneIndex);

			// We need to make sure that any bone that has this old bone as a parent is fixed up
			bRemovedBones = true;
		}
	}

	// If we've removed bones, we need to rebuild our name table.
	if (bRemovedBones || (RawNameToIndexMap.Num() == 0))
	{
		const USkeleton* Skeleton = Cast<USkeleton>(Requester);
		if (!Skeleton)
		{
			if (const USkeletalMesh* Mesh = Cast<USkeletalMesh>(Requester))
			{
				Skeleton = Mesh->Skeleton;
			}
			else
			{
				UE_LOG(LogAnimation, Warning, TEXT("RemoveDuplicateBones: Object supplied as requester (%s) needs to be either Skeleton or SkeletalMesh"), *GetFullNameSafe(Requester));
			}
		}

		// Additionally normalize all quaternions to be safe.
		for (int32 BoneIndex = 0; BoneIndex < GetRawBoneNum(); BoneIndex++)
		{
			RawRefBonePose[BoneIndex].NormalizeRotation();
		}

		const bool bRebuildNameMap = true;
		RebuildRefSkeleton(Skeleton, bRebuildNameMap);
	}

	// Make sure our arrays are in sync.
	checkSlow((RawRefBoneInfo.Num() == RawRefBonePose.Num()) && (RawRefBoneInfo.Num() == RawNameToIndexMap.Num()));
}

void FReferenceSkeleton::RebuildNameToIndexMap()
{
	// Start by clearing the current map.
	RawNameToIndexMap.Empty();

	// Then iterate over each bone, adding the name and bone index.
	const int32 NumBones = RawRefBoneInfo.Num();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		const FName& BoneName = RawRefBoneInfo[BoneIndex].Name;
		if (BoneName != NAME_None)
		{
			RawNameToIndexMap.Add(BoneName, BoneIndex);
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("RebuildNameToIndexMap: Bone with no name detected for index: %d"), BoneIndex);
		}
	}

	// Make sure we don't have duplicate bone names. This would be very bad.
	checkSlow(RawNameToIndexMap.Num() == NumBones);
}

SIZE_T FReferenceSkeleton::GetDataSize() const
{
	SIZE_T ResourceSize = 0;

	ResourceSize += RawRefBoneInfo.GetAllocatedSize();
	ResourceSize += RawRefBonePose.GetAllocatedSize();

	ResourceSize += FinalRefBoneInfo.GetAllocatedSize();
	ResourceSize += FinalRefBonePose.GetAllocatedSize();

	ResourceSize += RawNameToIndexMap.GetAllocatedSize();
	ResourceSize += FinalNameToIndexMap.GetAllocatedSize();

	return ResourceSize;
}

struct FEnsureParentsExistScratchArea : public TThreadSingleton<FEnsureParentsExistScratchArea>
{
	TArray<bool> BoneExists;
};

void FReferenceSkeleton::EnsureParentsExist(TArray<FBoneIndexType>& InOutBoneSortedArray) const
{
	const int32 NumBones = GetNum();
	// Iterate through existing array.
	int32 i = 0;

	TArray<bool>& BoneExists = FEnsureParentsExistScratchArea::Get().BoneExists;
	BoneExists.Reset();
	BoneExists.SetNumZeroed(NumBones);

	while (i < InOutBoneSortedArray.Num())
	{
		const int32 BoneIndex = InOutBoneSortedArray[i];

		// For the root bone, just move on.
		if (BoneIndex > 0)
		{
#if	!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Warn if we're getting bad data.
			// Bones are matched as int32, and a non found bone will be set to INDEX_NONE == -1
			// This should never happen, so if it does, something is wrong!
			if (BoneIndex >= NumBones)
			{
				UE_LOG(LogAnimation, Log, TEXT("FAnimationRuntime::EnsureParentsExist, BoneIndex >= RefSkeleton.GetNum()."));
				i++;
				continue;
			}
#endif
			BoneExists[BoneIndex] = true;

			const int32 ParentIndex = GetParentIndex(BoneIndex);

			// If we do not have this parent in the array, we add it in this location, and leave 'i' where it is.
			// This can happen if somebody removes bones in the physics asset, then it will try add back in, and in the process, 
			// parent can be missing
			if (!BoneExists[ParentIndex])
			{
				InOutBoneSortedArray.InsertUninitialized(i);
				InOutBoneSortedArray[i] = ParentIndex;
				BoneExists[ParentIndex] = true;
			}
			// If parent was in array, just move on.
			else
			{
				i++;
			}
		}
		else
		{
			BoneExists[0] = true;
			i++;
		}
	}
}

void FReferenceSkeleton::EnsureParentsExistAndSort(TArray<FBoneIndexType>& InOutBoneUnsortedArray) const
{
	InOutBoneUnsortedArray.Sort();

	EnsureParentsExist(InOutBoneUnsortedArray);

	InOutBoneUnsortedArray.Sort();
}

FArchive & operator<<(FArchive & Ar, FReferenceSkeleton & F)
{
	Ar << F.RawRefBoneInfo;
	Ar << F.RawRefBonePose;

	if (Ar.UE4Ver() >= VER_UE4_REFERENCE_SKELETON_REFACTOR)
	{
		Ar << F.RawNameToIndexMap;
	}

	// Fix up any assets that don't have an INDEX_NONE parent for Bone[0]
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FIXUP_ROOTBONE_PARENT)
	{
		if ((F.RawRefBoneInfo.Num() > 0) && (F.RawRefBoneInfo[0].ParentIndex != INDEX_NONE))
		{
			F.RawRefBoneInfo[0].ParentIndex = INDEX_NONE;
		}
	}

	if (Ar.IsLoading())
	{
		F.FinalRefBoneInfo = F.RawRefBoneInfo;
		F.FinalRefBonePose = F.RawRefBonePose;
		F.FinalNameToIndexMap = F.RawNameToIndexMap;
	}

	return Ar;
}

