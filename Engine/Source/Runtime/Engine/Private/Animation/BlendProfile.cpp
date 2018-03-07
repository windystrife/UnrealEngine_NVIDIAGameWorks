// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/BlendProfile.h"

UBlendProfile::UBlendProfile()
	: OwningSkeleton(nullptr)
{
	// Set up our owning skeleton and initialise bone references
	if(USkeleton* OuterAsSkeleton = Cast<USkeleton>(GetOuter()))
	{
		SetSkeleton(OuterAsSkeleton);
	}
}

void UBlendProfile::SetBoneBlendScale(int32 InBoneIdx, float InScale, bool bRecurse, bool bCreate)
{
	// Set the requested bone, then children if necessary
	SetSingleBoneBlendScale(InBoneIdx, InScale, bCreate);

	if(bRecurse)
	{
		const FReferenceSkeleton& RefSkeleton = OwningSkeleton->GetReferenceSkeleton();
		const int32 NumBones = RefSkeleton.GetNum();
		for(int32 ChildIdx = InBoneIdx + 1 ; ChildIdx < NumBones ; ++ChildIdx)
		{
			if(RefSkeleton.BoneIsChildOf(ChildIdx, InBoneIdx))
			{
				SetSingleBoneBlendScale(ChildIdx, InScale, bCreate);
			}
		}
	}
}

void UBlendProfile::SetBoneBlendScale(const FName& InBoneName, float InScale, bool bRecurse, bool bCreate)
{
	int32 BoneIndex = OwningSkeleton->GetReferenceSkeleton().FindBoneIndex(InBoneName);

	SetBoneBlendScale(BoneIndex, InScale, bRecurse, bCreate);
}

float UBlendProfile::GetBoneBlendScale(int32 InBoneIdx) const
{
	const FBlendProfileBoneEntry* FoundEntry = ProfileEntries.FindByPredicate([InBoneIdx](const FBlendProfileBoneEntry& Entry)
	{
		return Entry.BoneReference.BoneIndex == InBoneIdx;
	});

	if(FoundEntry)
	{
		return FoundEntry->BlendScale;
	}

	return 1.0f;
}

float UBlendProfile::GetBoneBlendScale(const FName& InBoneName) const
{
	const FBlendProfileBoneEntry* FoundEntry = ProfileEntries.FindByPredicate([InBoneName](const FBlendProfileBoneEntry& Entry)
	{
		return Entry.BoneReference.BoneName == InBoneName;
	});

	if(FoundEntry)
	{
		return FoundEntry->BlendScale;
	}

	return 1.0f;
}

void UBlendProfile::SetSkeleton(USkeleton* InSkeleton)
{
	OwningSkeleton = InSkeleton;

	if(OwningSkeleton)
	{
		// Initialise Current profile entries
		for(FBlendProfileBoneEntry& Entry : ProfileEntries)
		{
			Entry.BoneReference.Initialize(OwningSkeleton);
		}
	}
}

void UBlendProfile::PostLoad()
{
	Super::PostLoad();

	if(OwningSkeleton)
	{
		// Initialise Current profile entries
		for(FBlendProfileBoneEntry& Entry : ProfileEntries)
		{
			Entry.BoneReference.Initialize(OwningSkeleton);
		}
	}
}

int32 UBlendProfile::GetEntryIndex(const int32 InBoneIdx) const
{
	for(int32 Idx = 0 ; Idx < ProfileEntries.Num() ; ++Idx)
	{
		const FBlendProfileBoneEntry& Entry = ProfileEntries[Idx];
		if(Entry.BoneReference.BoneIndex == InBoneIdx)
		{
			return Idx;
		}
	}
	return INDEX_NONE;
}

int32 UBlendProfile::GetEntryIndex(const FName& InBoneName) const
{
	for(int32 Idx = 0 ; Idx < ProfileEntries.Num() ; ++Idx)
	{
		const FBlendProfileBoneEntry& Entry = ProfileEntries[Idx];
		if(Entry.BoneReference.BoneName == InBoneName)
		{
			return Idx;
		}
	}
	return INDEX_NONE;
}

float UBlendProfile::GetEntryBlendScale(const int32 InEntryIdx) const
{
	if(ProfileEntries.IsValidIndex(InEntryIdx))
	{
		return ProfileEntries[InEntryIdx].BlendScale;
	}
	// No overriden blend scale, return no scale
	return 1.0f;
}

int32 UBlendProfile::GetPerBoneInterpolationIndex(int32 BoneIndex, const FBoneContainer& RequiredBones) const
{
	// Our internal entries are skeleton bone indices, but we have pose indices here - convert to skeleton
	int32 ActualBoneIndex = RequiredBones.GetPoseToSkeletonBoneIndexArray()[BoneIndex];

	return GetEntryIndex(ActualBoneIndex);
}

void UBlendProfile::SetSingleBoneBlendScale(int32 InBoneIdx, float InScale, bool bCreate /*= false*/)
{
	FBlendProfileBoneEntry* Entry = ProfileEntries.FindByPredicate([InBoneIdx](const FBlendProfileBoneEntry& InEntry)
	{
		return InEntry.BoneReference.BoneIndex == InBoneIdx;
	});

	if(!Entry && bCreate)
	{
		Entry = &ProfileEntries[ProfileEntries.AddZeroed()];
		Entry->BoneReference.BoneName = OwningSkeleton->GetReferenceSkeleton().GetBoneName(InBoneIdx);
		Entry->BoneReference.Initialize(OwningSkeleton);
		Entry->BlendScale = InScale;
	}

	if(Entry)
	{
		Modify();
		Entry->BlendScale = InScale;

		// Remove any entry that gets set back to 1.0f - so we only store entries that actually contain a scale
		if(Entry->BlendScale == 1.0f)
		{
			ProfileEntries.RemoveAll([InBoneIdx](const FBlendProfileBoneEntry& Current)
			{
				return Current.BoneReference.BoneIndex == InBoneIdx;
			});
		}
	}
}
