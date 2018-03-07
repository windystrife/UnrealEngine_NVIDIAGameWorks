// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsAsset.cpp
=============================================================================*/ 

#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "UObject/ReleaseObjectVersion.h"
#include "Logging/MessageLog.h"
#include "UObjectIterator.h"

#if WITH_EDITOR
#include "Misc/MessageDialog.h"
#endif

#define LOCTEXT_NAMESPACE "PhysicsAsset"

///////////////////////////////////////	
//////////// UPhysicsAsset ////////////
///////////////////////////////////////

UPhysicsAsset::UPhysicsAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsAsset::UpdateBoundsBodiesArray()
{
	BoundsBodies.Empty();

	for(int32 i=0; i<SkeletalBodySetups.Num(); i++)
	{
		check(SkeletalBodySetups[i]);
		if(SkeletalBodySetups[i]->bConsiderForBounds)
		{
			BoundsBodies.Add(i);
		}
	}
}

#if WITH_EDITOR
void USkeletalBodySetup::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UPhysicsAsset* OwningPhysAsset = Cast<UPhysicsAsset>(GetOuter());

	if(PropertyChangedEvent.Property == nullptr || !OwningPhysAsset)
	{
		return;
	}

	if (FPhysicalAnimationProfile* PhysProfile = FindPhysicalAnimationProfile(OwningPhysAsset->CurrentPhysicalAnimationProfileName))
	{
			//changed any setting so copy dummy UI into profile location
			PhysProfile->PhysicalAnimationData = CurrentPhysicalAnimationProfile.PhysicalAnimationData;
	}

	OwningPhysAsset->RefreshPhysicsAssetChange();
}

FName USkeletalBodySetup::GetCurrentPhysicalAnimationProfileName() const
{
	FName CurrentProfileName;
	if(UPhysicsAsset* OwningPhysAsset = Cast<UPhysicsAsset>(GetOuter()))
	{
		CurrentProfileName = OwningPhysAsset->CurrentPhysicalAnimationProfileName;
	}

	return CurrentProfileName;
}

void USkeletalBodySetup::AddPhysicalAnimationProfile(FName ProfileName)
{
	FPhysicalAnimationProfile* NewProfile = new (PhysicalAnimationData) FPhysicalAnimationProfile();
	NewProfile->ProfileName = ProfileName;
}

void USkeletalBodySetup::RemovePhysicalAnimationProfile(FName ProfileName)
{
	for(int32 ProfileIdx = 0; ProfileIdx < PhysicalAnimationData.Num(); ++ProfileIdx)
	{
		if(PhysicalAnimationData[ProfileIdx].ProfileName == ProfileName)
		{
			PhysicalAnimationData.RemoveAtSwap(ProfileIdx--);
		}
	}
}

void USkeletalBodySetup::UpdatePhysicalAnimationProfiles(const TArray<FName>& Profiles)
{
	for(int32 ProfileIdx = 0; ProfileIdx < PhysicalAnimationData.Num(); ++ProfileIdx)
	{
		if(Profiles.Contains(PhysicalAnimationData[ProfileIdx].ProfileName) == false)
		{
			PhysicalAnimationData.RemoveAtSwap(ProfileIdx--);
		}
	}
}

void USkeletalBodySetup::DuplicatePhysicalAnimationProfile(FName DuplicateFromName, FName DuplicateToName)
{
	for (FPhysicalAnimationProfile& ProfileHandle : PhysicalAnimationData)
	{
		if (ProfileHandle.ProfileName == DuplicateFromName)
		{
			FPhysicalAnimationProfile* Duplicate = new (PhysicalAnimationData) FPhysicalAnimationProfile(ProfileHandle);
			Duplicate->ProfileName = DuplicateToName;
			break;
		}
	}
}

void USkeletalBodySetup::RenamePhysicalAnimationProfile(FName CurrentName, FName NewName)
{
	for(FPhysicalAnimationProfile& ProfileHandle : PhysicalAnimationData)
	{
		if(ProfileHandle.ProfileName == CurrentName)
		{
			ProfileHandle.ProfileName = NewName;
		}
	}
}

#endif

void UPhysicsAsset::UpdateBodySetupIndexMap()
{
	// update BodySetupIndexMap
	BodySetupIndexMap.Empty();

	for(int32 i=0; i<SkeletalBodySetups.Num(); i++)
	{
		check(SkeletalBodySetups[i]);
		BodySetupIndexMap.Add(SkeletalBodySetups[i]->BoneName, i);
	}
}

void UPhysicsAsset::PostLoad()
{
	Super::PostLoad();

	if(GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::PhysAssetUseSkeletalBodySetup)
	{
		SkeletalBodySetups.AddUninitialized(BodySetup_DEPRECATED.Num());
		for(int32 Idx = 0; Idx < BodySetup_DEPRECATED.Num(); ++Idx)
		{
			//Used to use BodySetup for physics asset, but moved to more specialized SkeletalBodySetup
			SkeletalBodySetups[Idx] = NewObject<USkeletalBodySetup>(this, NAME_None);
			
			TArray<uint8> OldData;
			FObjectWriter ObjWriter(BodySetup_DEPRECATED[Idx], OldData);
			FObjectReader ObjReader(SkeletalBodySetups[Idx], OldData);
		}
		
		BodySetup_DEPRECATED.Empty();
	}

	// Ensure array of bounds bodies is up to date.
	if(SkeletalBodySetups.Num() == 0)
	{
		UpdateBoundsBodiesArray();
	}

	if (SkeletalBodySetups.Num() > 0 && BodySetupIndexMap.Num() == 0)
	{
		UpdateBodySetupIndexMap();
	}

	if (GetLinkerCustomVersion(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::NoSyncAsyncPhysAsset)
	{
		bool bCurrentUseAsync = false;
		bool bAnyConflicts = false;
		for (int32 BodySetupIdx = 0; BodySetupIdx < SkeletalBodySetups.Num(); ++BodySetupIdx)
		{
			if(UBodySetup* BS = SkeletalBodySetups[BodySetupIdx])
			{
				if(BodySetupIdx == 0)
				{
					bCurrentUseAsync = BS->DefaultInstance.bUseAsyncScene;
				}
				else if(BS->DefaultInstance.bUseAsyncScene != bCurrentUseAsync)
				{
					bAnyConflicts = true;
					break;
				}
			}
		}

		bUseAsyncScene = bAnyConflicts ? false : bCurrentUseAsync;	//If there's any conflict just use the sync scene

		for (UBodySetup* BS : SkeletalBodySetups)
		{
			if (BS)
			{
				BS->DefaultInstance.bUseAsyncScene = bUseAsyncScene;
			}
		}

		
#if WITH_EDITOR
		if(bAnyConflicts)
		{
			FMessageLog("LoadErrors").Warning(FText::Format(LOCTEXT("ConflictSyncAsync", "Physics Asset had both sync and async bodies. Defaulting to sync scene only. If you'd like to use async change UseAsyncScene on the PhysicsAsset:{0}"),
				FText::FromString(GetName())));
		}
#endif
	}
}

void UPhysicsAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CollisionDisableTable;

#if WITH_EDITORONLY_DATA
	if (DefaultSkelMesh_DEPRECATED != NULL)
	{
		PreviewSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(DefaultSkelMesh_DEPRECATED);
		DefaultSkelMesh_DEPRECATED = NULL;
	}
#endif

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
}


void UPhysicsAsset::EnableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
	if(BodyIndexA == BodyIndexB)
	{
		return;
	}

	FRigidBodyIndexPair Key(BodyIndexA, BodyIndexB);

	// If its not in table - do nothing
	if( !CollisionDisableTable.Find(Key) )
	{
		return;
	}

	CollisionDisableTable.Remove(Key);
}

void UPhysicsAsset::DisableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
	if(BodyIndexA == BodyIndexB)
	{
		return;
	}

	FRigidBodyIndexPair Key(BodyIndexA, BodyIndexB);

	// If its already in the disable table - do nothing
	if( CollisionDisableTable.Find(Key) )
	{
		return;
	}

	CollisionDisableTable.Add(Key, 0);
}

bool UPhysicsAsset::IsCollisionEnabled(int32 BodyIndexA, int32 BodyIndexB) const
{
	if(BodyIndexA == BodyIndexB)
	{
		return false;
	}

	if(CollisionDisableTable.Find(FRigidBodyIndexPair(BodyIndexA, BodyIndexB)))
	{
		return false;
	}

	return true;
}

FBox UPhysicsAsset::CalcAABB(const USkinnedMeshComponent* MeshComp, const FTransform& LocalToWorld) const
{
	FBox Box(ForceInit);

	if (!MeshComp)
	{
		return Box;
	}

	FVector Scale3D = LocalToWorld.GetScale3D();
	if( Scale3D.IsUniform() )
	{
		const TArray<int32>* BodyIndexRefs = NULL;
		TArray<int32> AllBodies;
		// If we want to consider all bodies, make array with all body indices in
		if(MeshComp->bConsiderAllBodiesForBounds)
		{
			AllBodies.AddUninitialized(SkeletalBodySetups.Num());
			for(int32 i=0; i<SkeletalBodySetups.Num();i ++)
			{
				AllBodies[i] = i;
			}
			BodyIndexRefs = &AllBodies;
		}
		// Otherwise, use the cached shortlist of bodies to consider
		else
		{
			BodyIndexRefs = &BoundsBodies;
		}

		// Then iterate over bodies we want to consider, calculating bounding box for each
		const int32 BodySetupNum = (*BodyIndexRefs).Num();

		for(int32 i=0; i<BodySetupNum; i++)
		{
			const int32 BodyIndex = (*BodyIndexRefs)[i];
			UBodySetup* bs = SkeletalBodySetups[BodyIndex];

			// Check if setup should be considered for bounds, or if all bodies should be considered anyhow
			if (bs->bConsiderForBounds || MeshComp->bConsiderAllBodiesForBounds)
			{
				if (i+1<BodySetupNum)
				{
					int32 NextIndex = (*BodyIndexRefs)[i+1];
					FPlatformMisc::Prefetch(SkeletalBodySetups[NextIndex]);
					FPlatformMisc::Prefetch(SkeletalBodySetups[NextIndex], PLATFORM_CACHE_LINE_SIZE);
				}

				int32 BoneIndex = MeshComp->GetBoneIndex(bs->BoneName);
				if(BoneIndex != INDEX_NONE)
				{
					const FTransform WorldBoneTransform = MeshComp->GetBoneTransform(BoneIndex, LocalToWorld);
					const FBox BodySetupBounds = bs->AggGeom.CalcAABB(WorldBoneTransform);
					
					Box += BodySetupBounds;
				}
			}
		}
	}
	else
	{
		UE_LOG(LogPhysics, Log,  TEXT("UPhysicsAsset::CalcAABB : Non-uniform scale factor. You will not be able to collide with it.  Turn off collision and wrap it with a blocking volume.  MeshComp: %s  SkelMesh: %s"), *MeshComp->GetFullName(), MeshComp->SkeletalMesh ? *MeshComp->SkeletalMesh->GetFullName() : TEXT("NULL") );
	}

	if(!Box.IsValid)
	{
		Box = FBox( LocalToWorld.GetLocation(), LocalToWorld.GetLocation() );
	}

	const float MinBoundSize = 1.f;
	const FVector BoxSize = Box.GetSize();

	if(BoxSize.GetMin() < MinBoundSize)
	{
		const FVector ExpandByDelta ( FMath::Max(0.f, MinBoundSize - BoxSize.X), FMath::Max(0.f, MinBoundSize - BoxSize.Y), FMath::Max(0.f, MinBoundSize - BoxSize.Z) );
		Box = Box.ExpandBy(ExpandByDelta * 0.5f);	//expand by applies to both directions with GetSize applies to total size so divide by 2
	}

	return Box;
}

#if WITH_EDITOR
bool UPhysicsAsset::CanCalculateValidAABB(const USkinnedMeshComponent* MeshComp, const FTransform& LocalToWorld) const
{
	bool ValidBox = false;

	if (!MeshComp)
	{
		return false;
	}

	FVector Scale3D = LocalToWorld.GetScale3D();
	if (!Scale3D.IsUniform())
	{
		return false;
	}

	for (int32 i = 0; i < SkeletalBodySetups.Num(); i++)
	{
		UBodySetup* bs = SkeletalBodySetups[i];
		// Check if setup should be considered for bounds, or if all bodies should be considered anyhow
		if (bs->bConsiderForBounds || MeshComp->bConsiderAllBodiesForBounds)
		{
			int32 BoneIndex = MeshComp->GetBoneIndex(bs->BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				FTransform WorldBoneTransform = MeshComp->GetBoneTransform(BoneIndex, LocalToWorld);
				if (FMath::Abs(WorldBoneTransform.GetDeterminant()) >(float)KINDA_SMALL_NUMBER)
				{
					FBox Box = bs->AggGeom.CalcAABB(WorldBoneTransform);
					if (Box.GetSize().SizeSquared() > (float)KINDA_SMALL_NUMBER)
					{
						ValidBox = true;
						break;
					}
				}
			}
		}
	}
	return ValidBox;
}
#endif //WITH_EDITOR

int32	UPhysicsAsset::FindControllingBodyIndex(class USkeletalMesh* skelMesh, int32 StartBoneIndex)
{
	int32 BoneIndex = StartBoneIndex;
	while(BoneIndex!=INDEX_NONE)
	{
		FName BoneName = skelMesh->RefSkeleton.GetBoneName(BoneIndex);
		int32 BodyIndex = FindBodyIndex(BoneName);

		if(BodyIndex != INDEX_NONE)
			return BodyIndex;

		int32 ParentBoneIndex = skelMesh->RefSkeleton.GetParentIndex(BoneIndex);

		if(ParentBoneIndex == BoneIndex)
			return INDEX_NONE;

		BoneIndex = ParentBoneIndex;
	}

	return INDEX_NONE; // Shouldn't reach here.
}

int32 UPhysicsAsset::FindParentBodyIndex(class USkeletalMesh * skelMesh, int32 StartBoneIndex) const
{
	int32 BoneIndex = StartBoneIndex;
	while ((BoneIndex = skelMesh->RefSkeleton.GetParentIndex(BoneIndex)) != INDEX_NONE)
	{
		FName BoneName = skelMesh->RefSkeleton.GetBoneName(BoneIndex);
		int32 BodyIndex = FindBodyIndex(BoneName);

		if (StartBoneIndex == BoneIndex)
			return INDEX_NONE;

		if (BodyIndex != INDEX_NONE)
			return BodyIndex;
	}

	return INDEX_NONE;
}


int32 UPhysicsAsset::FindBodyIndex(FName bodyName) const
{
	const int32* IdxData = BodySetupIndexMap.Find(bodyName);
	if (IdxData)
	{
		return *IdxData;
	}

	return INDEX_NONE;
}

int32 UPhysicsAsset::FindConstraintIndex(FName ConstraintName)
{
	for(int32 i=0; i<ConstraintSetup.Num(); i++)
	{
		if( ConstraintSetup[i]->DefaultInstance.JointName == ConstraintName )
		{
			return i;
		}
	}

	return INDEX_NONE;
}

FName UPhysicsAsset::FindConstraintBoneName(int32 ConstraintIndex)
{
	if ( (ConstraintIndex < 0) || (ConstraintIndex >= ConstraintSetup.Num()) )
	{
		return NAME_None;
	}

	return ConstraintSetup[ConstraintIndex]->DefaultInstance.JointName;
}

int32 UPhysicsAsset::FindMirroredBone(class USkeletalMesh* skelMesh, int32 BoneIndex)
{
	if (BoneIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	//we try to find the mirroed bone using several approaches. The first is to look for the same name but with _R instead of _L or vise versa
	FName BoneName = skelMesh->RefSkeleton.GetBoneName(BoneIndex);
	FString BoneNameString = BoneName.ToString();

	bool bIsLeft = BoneNameString.Find("_L", ESearchCase::IgnoreCase, ESearchDir::FromEnd) == (BoneNameString.Len() - 2);	//has _L at the end
	bool bIsRight = BoneNameString.Find("_R", ESearchCase::IgnoreCase, ESearchDir::FromEnd) == (BoneNameString.Len() - 2);	//has _R at the end
	bool bMirrorConvention = bIsLeft || bIsRight;

	//if the bone follows our left right naming convention then let's try and find its mirrored bone
	int32 BoneIndexMirrored = INDEX_NONE;
	if (bMirrorConvention)
	{
		FString BoneNameMirrored = BoneNameString.LeftChop(2);
		BoneNameMirrored.Append(bIsLeft ? "_R" : "_L");

		BoneIndexMirrored = skelMesh->RefSkeleton.FindBoneIndex(FName(*BoneNameMirrored));
	}

	return BoneIndexMirrored;
}

void UPhysicsAsset::GetBodyIndicesBelow(TArray<int32>& OutBodyIndices, FName InBoneName, USkeletalMesh* SkelMesh, bool bIncludeParent /*= true*/)
{
	int32 BaseIndex = SkelMesh->RefSkeleton.FindBoneIndex(InBoneName);

	// Iterate over all other bodies, looking for 'children' of this one
	for(int32 i=0; i<SkeletalBodySetups.Num(); i++)
	{
		UBodySetup* BS = SkeletalBodySetups[i];
		FName TestName = BS->BoneName;
		int32 TestIndex = SkelMesh->RefSkeleton.FindBoneIndex(TestName);

		if( (bIncludeParent && TestIndex == BaseIndex) || SkelMesh->RefSkeleton.BoneIsChildOf(TestIndex, BaseIndex))
		{
			OutBodyIndices.Add(i);
		}
	}
}

void UPhysicsAsset::GetNearestBodyIndicesBelow(TArray<int32> & OutBodyIndices, FName InBoneName, USkeletalMesh * InSkelMesh)
{
	TArray<int32> AllBodiesBelow;
	GetBodyIndicesBelow(AllBodiesBelow, InBoneName, InSkelMesh, false);

	//we need to filter all bodies below to first in the chain
	TArray<bool> Nearest;
	Nearest.AddUninitialized(SkeletalBodySetups.Num());
	for (int32 i = 0; i < Nearest.Num(); ++i)
	{
		Nearest[i] = true;
	}

	for (int32 i = 0; i < AllBodiesBelow.Num(); i++)
	{
		int32 BodyIndex = AllBodiesBelow[i];
		if (Nearest[BodyIndex] == false) continue;

		UBodySetup * Body = SkeletalBodySetups[BodyIndex];
		TArray<int32> BodiesBelowMe;
		GetBodyIndicesBelow(BodiesBelowMe, Body->BoneName, InSkelMesh, false);
		
		for (int j = 0; j < BodiesBelowMe.Num(); ++j)
		{
			Nearest[BodiesBelowMe[j]] = false;	
		}
	}

	for (int32 i = 0; i < AllBodiesBelow.Num(); i++)
	{
		int32 BodyIndex = AllBodiesBelow[i];
		if (Nearest[BodyIndex])
		{
			OutBodyIndices.Add(BodyIndex);
		}
	}
}

void UPhysicsAsset::ClearAllPhysicsMeshes()
{
	for(int32 i=0; i<SkeletalBodySetups.Num(); i++)
	{
		SkeletalBodySetups[i]->ClearPhysicsMeshes();
	}
}

#if WITH_EDITOR

void UPhysicsAsset::InvalidateAllPhysicsMeshes()
{
	for(int32 i=0; i<SkeletalBodySetups.Num(); i++)
	{
		SkeletalBodySetups[i]->InvalidatePhysicsData();
	}
}


void UPhysicsAsset::PostEditUndo()
{
	Super::PostEditUndo();
	
	UpdateBodySetupIndexMap();
	UpdateBoundsBodiesArray();
}

void UPhysicsAsset::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	PreConstraintProfiles = ConstraintProfiles;
	PrePhysicalAnimationProfiles = PhysicalAnimationProfiles;
}

template <typename T>
void SanitizeProfilesHelper(const TArray<T*>& SetupInstances, const TArray<FName>& PreProfiles, TArray<FName>& PostProfiles, FPropertyChangedEvent& PropertyChangedEvent, const FName PropertyName, FName& CurrentProfileName, TFunctionRef<void (T*, FName, FName)> RenameFunc, TFunctionRef<void(T*, FName, FName)> DuplicateFunc, TFunctionRef<void(T*, const TArray<FName>& )> UpdateFunc)
{
	const int32 ArrayIdx = PropertyChangedEvent.GetArrayIndex(PropertyName.ToString());
	const FName OldName = PreProfiles.IsValidIndex(ArrayIdx) ? PreProfiles[ArrayIdx] : NAME_None;

	if (ArrayIdx != INDEX_NONE)
	{
		if(PropertyChangedEvent.ChangeType != EPropertyChangeType::Unspecified && PropertyChangedEvent.ChangeType != EPropertyChangeType::ArrayRemove)
		{
			int32 CollisionCount = 0;
			FName NewName = PostProfiles[ArrayIdx] == NAME_None ? FName(TEXT("New")) : PostProfiles[ArrayIdx];
			const FString NewNameNoNumber = NewName.ToString();
			while(PreProfiles.Contains(NewName))
			{
				FString NewNameStr = NewNameNoNumber;
				NewNameStr.Append("_").AppendInt(++CollisionCount);
				NewName = FName(*NewNameStr);
			}

			PostProfiles[ArrayIdx] = NewName;
		}
	}
	

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet || PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
	{
		if (ArrayIdx != INDEX_NONE)	//INDEX_NONE can come when emptying the array, so just ignore it
		{
			for (T* SetupInstance : SetupInstances)
			{
				SetupInstance->Modify();
				RenameFunc(SetupInstance, PreProfiles[ArrayIdx], PostProfiles[ArrayIdx]);
			}

			if (CurrentProfileName == PreProfiles[ArrayIdx])
			{
				CurrentProfileName = PostProfiles[ArrayIdx];
			}
		}
	}

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
	{
		for (T* SetupInstance : SetupInstances)
		{
			SetupInstance->Modify();
			DuplicateFunc(SetupInstance, OldName, PostProfiles[ArrayIdx]);
		}
	}

	//array events like empty currently do not get an Empty type so we need to do this final sanitization every time something changes just in case
	{
		//delete requires removing old profiles
		for (T* SetupInstance : SetupInstances)
		{
			SetupInstance->Modify();
			UpdateFunc(SetupInstance, PostProfiles);
		}

		if (PostProfiles.Find(CurrentProfileName) == INDEX_NONE)
		{
			CurrentProfileName = NAME_None;
		}
	}
}

void UPhysicsAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(UProperty* Property = PropertyChangedEvent.Property)
	{
		const FName PropertyName = Property->GetFName();
		if(PropertyName == GET_MEMBER_NAME_CHECKED(UPhysicsAsset, PhysicalAnimationProfiles))
		{
			auto RenameFunc = [](USkeletalBodySetup* BS, FName PreName, FName NewName)
			{
				BS->RenamePhysicalAnimationProfile(PreName, NewName);
			};

			auto DuplicateFunc = [](USkeletalBodySetup* BS, FName DuplicateFromName, FName DuplicateToName)
			{
				BS->DuplicatePhysicalAnimationProfile(DuplicateFromName, DuplicateToName);
			};

			auto UpdateFunc = [](USkeletalBodySetup* BS, const TArray<FName>& NewProfiles)
			{
				BS->UpdatePhysicalAnimationProfiles(NewProfiles);
			};

			SanitizeProfilesHelper<USkeletalBodySetup>(SkeletalBodySetups, PrePhysicalAnimationProfiles, PhysicalAnimationProfiles, PropertyChangedEvent, PropertyName, CurrentPhysicalAnimationProfileName, RenameFunc, DuplicateFunc, UpdateFunc);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPhysicsAsset, ConstraintProfiles))
		{
			auto RenameFunc = [](UPhysicsConstraintTemplate* CS, FName PreName, FName NewName)
			{
				CS->RenameConstraintProfile(PreName, NewName);
			};

			auto DuplicateFunc = [](UPhysicsConstraintTemplate* CS, FName DuplicateFromName, FName DuplicateToName)
			{
				CS->DuplicateConstraintProfile(DuplicateFromName, DuplicateToName);
			};

			auto UpdateFunc = [](UPhysicsConstraintTemplate* CS, const TArray<FName>& NewProfiles)
			{
				CS->UpdateConstraintProfiles(NewProfiles);
			};

			SanitizeProfilesHelper<UPhysicsConstraintTemplate>(ConstraintSetup, PreConstraintProfiles, ConstraintProfiles, PropertyChangedEvent, PropertyName, CurrentConstraintProfileName, RenameFunc, DuplicateFunc, UpdateFunc);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPhysicsAsset, bUseAsyncScene))
		{
			for(USkeletalBodySetup* BS : SkeletalBodySetups)
			{
				BS->Modify();
				BS->DefaultInstance.bUseAsyncScene = bUseAsyncScene;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshPhysicsAssetChange();
}

#endif

//// THUMBNAIL SUPPORT //////

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UPhysicsAsset::GetDesc()
{
	return FString::Printf( TEXT("%d Bodies, %d Constraints"), SkeletalBodySetups.Num(), ConstraintSetup.Num() );
}

void UPhysicsAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add( FAssetRegistryTag("Bodies", FString::FromInt(SkeletalBodySetups.Num()), FAssetRegistryTag::TT_Numerical) );
	OutTags.Add( FAssetRegistryTag("Constraints", FString::FromInt(ConstraintSetup.Num()), FAssetRegistryTag::TT_Numerical) );

	Super::GetAssetRegistryTags(OutTags);
}


void UPhysicsAsset::BodyFindConstraints(int32 BodyIndex, TArray<int32>& Constraints)
{
	Constraints.Empty();
	FName BodyName = SkeletalBodySetups[BodyIndex]->BoneName;

	for(int32 ConIdx=0; ConIdx<ConstraintSetup.Num(); ConIdx++)
	{
		if( ConstraintSetup[ConIdx]->DefaultInstance.ConstraintBone1 == BodyName || ConstraintSetup[ConIdx]->DefaultInstance.ConstraintBone2 == BodyName )
		{
			Constraints.Add(ConIdx);
		}
	}
}

#if WITH_EDITOR
//#nv begin #Blast Physics asset change broadcast
UPhysicsAsset::FRefreshPhysicsAssetChangeDelegate UPhysicsAsset::OnRefreshPhysicsAssetChange;
//nv end
void UPhysicsAsset::RefreshPhysicsAssetChange() const
{
	for (FObjectIterator Iter(USkeletalMeshComponent::StaticClass()); Iter; ++Iter)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*Iter);
		if (SkeletalMeshComponent->GetPhysicsAsset() == this)
		{
			// it needs to recreate IF it already has been created
			if (SkeletalMeshComponent->IsPhysicsStateCreated() && SkeletalMeshComponent->Bodies.Num() > 0)
			{
				SkeletalMeshComponent->RecreatePhysicsState();
				SkeletalMeshComponent->InvalidateCachedBounds();
				SkeletalMeshComponent->UpdateBounds();
				SkeletalMeshComponent->MarkRenderTransformDirty();
			}
		}
	}
//#nv begin #Blast Physics asset change broadcast
	OnRefreshPhysicsAssetChange.Broadcast(this);
//nv end
}

USkeletalMesh* UPhysicsAsset::GetPreviewMesh() const
{
	USkeletalMesh* PreviewMesh = PreviewSkeletalMesh.Get();
	if(!PreviewMesh)
	{
		// if preview mesh isn't loaded, see if we have set
		FSoftObjectPath PreviewMeshStringRef = PreviewSkeletalMesh.ToSoftObjectPath();
		// load it since now is the time to load
		if(!PreviewMeshStringRef.ToString().IsEmpty())
		{
			PreviewMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *PreviewMeshStringRef.ToString(), nullptr, LOAD_None, nullptr));
		}
	}

	return PreviewMesh;
}

void UPhysicsAsset::SetPreviewMesh(USkeletalMesh* PreviewMesh)
{
	if(PreviewMesh)
	{
		// See if any bones are missing from the skeletal mesh we are trying to use
		// @todo Could do more here - check for bone lengths etc. Maybe modify asset?
		for (int32 i = 0; i < SkeletalBodySetups.Num(); ++i)
		{
			FName BodyName = SkeletalBodySetups[i]->BoneName;
			int32 BoneIndex = PreviewMesh->RefSkeleton.FindBoneIndex(BodyName);
			if (BoneIndex == INDEX_NONE)
			{
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format( LOCTEXT("BoneMissingFromSkelMesh", "The SkeletalMesh is missing bone '{0}' needed by this PhysicsAsset."), FText::FromName(BodyName) ));
				return;
			}
		}
	}

	Modify();
	PreviewSkeletalMesh = PreviewMesh;
}

#endif

void UPhysicsAsset::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	for (const auto& SingleBody : SkeletalBodySetups)
	{
		SingleBody->GetResourceSizeEx(CumulativeResourceSize);
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(BodySetupIndexMap.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(CollisionDisableTable.GetAllocatedSize());

	// @todo implement inclusive mode
}

#undef LOCTEXT_NAMESPACE
