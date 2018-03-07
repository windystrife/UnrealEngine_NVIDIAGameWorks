// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 * This is the definition for a skeleton, used to animate USkeletalMesh
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "ReferenceSkeleton.h"
#include "Animation/PreviewAssetAttachComponent.h"
#include "Animation/SmartName.h"
#include "Engine/AssetUserData.h"
#include "Interfaces/Interface_AssetUserData.h"

#include "Skeleton.generated.h"

class UAnimSequence;
class UBlendProfile;
class URig;
class USkeletalMesh;
class USkeletalMeshSocket;

/** This is a mapping table between bone in a particular skeletal mesh and bone of this skeleton set. */
USTRUCT()
struct FSkeletonToMeshLinkup
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * Mapping table. Size must be same as size of bone tree (not Mesh Ref Pose). 
	 * No index should be more than the number of bones in this skeleton
	 * -1 indicates no match for this bone - will be ignored.
	 */
	UPROPERTY()
	TArray<int32> SkeletonToMeshTable;

	/** 
	 * Mapping table. Size must be same as size of ref pose (not bone tree). 
	 * No index should be more than the number of bones in this skeletalmesh
	 * -1 indicates no match for this bone - will be ignored.
	 */
	UPROPERTY()
	TArray<int32> MeshToSkeletonTable;

};

/** Bone translation retargeting mode. */
UENUM()
namespace EBoneTranslationRetargetingMode
{
	enum Type
	{
		/** Use translation from animation data. */
		Animation,
		/** Use fixed translation from Skeleton. */
		Skeleton,
		/** Use Translation from animation, but scale length by Skeleton's proportions. */
		AnimationScaled,
		/** Use Translation from animation, but also play the difference from retargeting pose as an additive. */
		AnimationRelative,
	};
}

/** Each Bone node in BoneTree */
USTRUCT()
struct FBoneNode
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone, this is the search criteria to match with mesh bone. This will be NAME_None if deleted. */
	UPROPERTY()
	FName Name_DEPRECATED;

	/** Parent Index. -1 if not used. The root has 0 as its parent. Do not delete the element but set this to -1. If it is revived by other reason, fix up this link. */
	UPROPERTY()
	int32 ParentIndex_DEPRECATED;

	/** Retargeting Mode for Translation Component. */
	UPROPERTY(EditAnywhere, Category=BoneNode)
	TEnumAsByte<EBoneTranslationRetargetingMode::Type> TranslationRetargetingMode;

	FBoneNode()
		: ParentIndex_DEPRECATED(INDEX_NONE)
		, TranslationRetargetingMode(EBoneTranslationRetargetingMode::Animation)
	{
	}

	FBoneNode(FName InBoneName, int32 InParentIndex)
		: Name_DEPRECATED(InBoneName)
		, ParentIndex_DEPRECATED(InParentIndex)
		, TranslationRetargetingMode(EBoneTranslationRetargetingMode::Animation)
	{
	}
};

/** This is a mapping table between bone in a particular skeletal mesh and bone of this skeleton set. */
USTRUCT()
struct FReferencePose
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName	PoseName;

	UPROPERTY()
	TArray<FTransform>	ReferencePose;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	USkeletalMesh*	ReferenceMesh;
#endif

	/**
	 * Serializes the bones
	 *
	 * @param Ar - The archive to serialize into.
	 * @param Rect - The bone container to serialize.
	 *
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FReferencePose & P);
};

USTRUCT()
struct FBoneReductionSetting
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FName> BonesToRemove;

	bool Add(FName BoneName)
	{
		if ( BoneName!=NAME_None && !BonesToRemove.Contains(BoneName) )
		{
			BonesToRemove.Add(BoneName);
			return true;
		}

		return false;
	}

	void Remove(FName BoneName)
	{
		BonesToRemove.Remove(BoneName);
	}

	bool Contains(FName BoneName)
	{
		return (BonesToRemove.Contains(BoneName));
	}
};

USTRUCT()
struct FNameMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName NodeName;

	UPROPERTY()
	FName BoneName;

	FNameMapping()
		: NodeName(NAME_None)
		, BoneName(NAME_None)
	{
	}

	FNameMapping(FName InNodeName)
		: NodeName(InNodeName)
		, BoneName(NAME_None)
	{
	}

	FNameMapping(FName InNodeName, FName InBoneName)
		: NodeName(InNodeName)
		, BoneName(InBoneName)
	{
	}
};

USTRUCT()
struct FRigConfiguration
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class URig * Rig;

	// @todo in the future we can make this to be run-time data
	UPROPERTY()
	TArray<FNameMapping> BoneMappingTable;
};

USTRUCT()
struct FAnimSlotGroup
{
	GENERATED_USTRUCT_BODY()

public:
	static ENGINE_API const FName DefaultGroupName;
	static ENGINE_API const FName DefaultSlotName;

	UPROPERTY()
	FName GroupName;

	UPROPERTY()
	TArray<FName> SlotNames;

	FAnimSlotGroup()
		: GroupName(DefaultGroupName)
	{
	}

	FAnimSlotGroup(FName InGroupName)
		: GroupName(InGroupName)
	{
	}
};

namespace VirtualBoneNameHelpers
{
	const FString VirtualBonePrefix(TEXT("VB "));

	ENGINE_API FString AddVirtualBonePrefix(const FString& InName);
	ENGINE_API FName RemoveVirtualBonePrefix(const FString& InName);
}

USTRUCT()
struct FVirtualBone
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	FName SourceBoneName;

	UPROPERTY()
	FName TargetBoneName;

	UPROPERTY()
	FName VirtualBoneName;

	FVirtualBone() {}

	FVirtualBone(FName InSource, FName InTarget)
		: SourceBoneName(InSource)
		, TargetBoneName(InTarget)
	{
		FString VBNameString = VirtualBoneNameHelpers::AddVirtualBonePrefix(SourceBoneName.ToString() + TEXT("_") + TargetBoneName.ToString());
		VirtualBoneName = FName(*VBNameString);
	}
};

/**
 *	USkeleton : that links between mesh and animation
 *		- Bone hierarchy for animations
 *		- Bone/track linkup between mesh and animation
 *		- Retargetting related
 *		- Mirror table
 */
UCLASS(hidecategories=Object, MinimalAPI)
class USkeleton : public UObject, public IInterface_AssetUserData
{
	friend class UAnimationBlueprintLibrary;

	GENERATED_UCLASS_BODY()

protected:
	/** Skeleton bone tree - each contains name and parent index**/
	UPROPERTY(VisibleAnywhere, Category=Skeleton)
	TArray<struct FBoneNode> BoneTree;

	/** Reference skeleton poses in local space */
	UPROPERTY()
	TArray<FTransform> RefLocalPoses_DEPRECATED;

	/** Reference Skeleton */
	FReferenceSkeleton ReferenceSkeleton;

	/** Guid for skeleton */
	FGuid Guid;

	/** Guid for virtual bones.
	 *  Separate so that we don't have to dirty the original guid when only changing virtual bones */
	UPROPERTY()
	FGuid VirtualBoneGuid;

	/** Conversion function. Remove when VER_UE4_REFERENCE_SKELETON_REFACTOR is removed. */
	void ConvertToFReferenceSkeleton();

	/**
	*  Array of this skeletons virtual bones. These are new bones are links between two existing bones
	*  and are baked into all the skeletons animations
	*/
	UPROPERTY()
	TArray<FVirtualBone> VirtualBones;

public:
	//~ Begin UObject Interface.
#if WITH_EDITOR
	ENGINE_API virtual void PreEditUndo() override;
	ENGINE_API virtual void PostEditUndo() override;
#endif

	/** Accessor to Reference Skeleton to make data read only */
	const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return ReferenceSkeleton;
	}

	/** Accessor for the array of virtual bones on this skeleton */
	const TArray<FVirtualBone>& GetVirtualBones() const { return VirtualBones; }

	/** 
	 *	Array of named socket locations, set up in editor and used as a shortcut instead of specifying 
	 *	everything explicitly to AttachComponent in the SkeletalMeshComponent.
	 */
	UPROPERTY()
	TArray<class USkeletalMeshSocket*> Sockets;

	/** Serializable retarget sources for this skeleton **/
	TMap< FName, FReferencePose > AnimRetargetSources;

	// Typedefs for greater smartname UID readability, add one for each smartname category 
	typedef SmartName::UID_Type AnimCurveUID;

	// Names for smartname mappings, if you're adding a new category of smartnames add a new name here
	static ENGINE_API const FName AnimCurveMappingName;

	// Names for smartname mappings, if you're adding a new category of smartnames add a new name here
	static ENGINE_API const FName AnimTrackCurveMappingName;

	// these return container of curve meta data, if you modify this container, 
	// you'll have to call REfreshCAchedAnimationCurveData to apply
	ENGINE_API FCurveMetaData* GetCurveMetaData(const FName& CurveName);
	ENGINE_API const FCurveMetaData* GetCurveMetaData(const FName& CurveName) const;
	ENGINE_API const FCurveMetaData* GetCurveMetaData(const SmartName::UID_Type CurveUID) const;
	ENGINE_API FCurveMetaData* GetCurveMetaData(const FSmartName& CurveName);
	ENGINE_API const FCurveMetaData* GetCurveMetaData(const FSmartName& CurveName) const;
	// this is called when you know both flags - called by post serialize
	ENGINE_API void AccumulateCurveMetaData(FName CurveName, bool bMaterialSet, bool bMorphtargetSet);

	ENGINE_API bool AddNewVirtualBone(const FName SourceBoneName, const FName TargetBoneName);

	ENGINE_API bool AddNewVirtualBone(const FName SourceBoneName, const FName TargetBoneName, FName& NewVirtualBoneName);

	ENGINE_API void RemoveVirtualBones(const TArray<FName>& BonesToRemove);

	ENGINE_API void RenameVirtualBone(const FName OriginalBoneName, const FName NewBoneName);
	
	void HandleVirtualBoneChanges();

	// return version of AnimCurveUidVersion
	uint16 GetAnimCurveUidVersion() const { return AnimCurveUidVersion;  }
	const TArray<AnimCurveUID>& GetDefaultCurveUIDList() const { return DefaultCurveUIDList; }
protected:
	// Container for smart name mappings
	UPROPERTY()
	FSmartNameContainer SmartNames;

	// this is default curve uid list used like ref pose, as default value
	// don't use this unless you want all curves from the skeleton
	// FBoneContainer contains only list that is used by current LOD
	TArray<AnimCurveUID> DefaultCurveUIDList;

private:
	/** Increase the AnimCurveUidVersion so that instances can get the latest information */
	void IncreaseAnimCurveUidVersion();
	/** Current  Anim Curve Uid Version. Increase whenever it has to be recalculated */
	uint16 AnimCurveUidVersion;

public:
	//////////////////////////////////////////////////////////////////////////
	// Blend Profiles

	/** List of blend profiles available in this skeleton */
	UPROPERTY(Instanced)
	TArray<UBlendProfile*> BlendProfiles;

	/** Get the specified blend profile by name */
	ENGINE_API UBlendProfile* GetBlendProfile(const FName& InProfileName);

	/** Create a new blend profile with the specified name */
	ENGINE_API UBlendProfile* CreateNewBlendProfile(const FName& InProfileName);

	//////////////////////////////////////////////////////////////////////////

	/************************************************************************/
	/* Slot Groups */
	/************************************************************************/
private:
	// serialized slot groups and slot names.
	UPROPERTY()
	TArray<FAnimSlotGroup> SlotGroups;

	/** SlotName to GroupName TMap, only at runtime, not serialized. **/
	TMap<FName, FName> SlotToGroupNameMap;

	void BuildSlotToGroupMap(bool bInRemoveDuplicates = false);

public:
	ENGINE_API FAnimSlotGroup* FindAnimSlotGroup(const FName& InGroupName);
	ENGINE_API const FAnimSlotGroup* FindAnimSlotGroup(const FName& InGroupName) const;
	ENGINE_API const TArray<FAnimSlotGroup>& GetSlotGroups() const;
	ENGINE_API bool ContainsSlotName(const FName& InSlotName) const;
	ENGINE_API void RegisterSlotNode(const FName& InSlotName);
	ENGINE_API void SetSlotGroupName(const FName& InSlotName, const FName& InGroupName);
	/** Returns true if Group is added, false if it already exists */
	ENGINE_API bool AddSlotGroupName(const FName& InNewGroupName);
	ENGINE_API FName GetSlotGroupName(const FName& InSlotName) const;

	// Edits/removes slot group data
	// WARNING: Does not verify that the names aren't used anywhere - if it isn't checked
	// by the caller the names will be recreated when referencing assets load again.
	ENGINE_API void RemoveSlotName(const FName& InSlotName);
	ENGINE_API void RemoveSlotGroup(const FName& InSlotName);
	ENGINE_API void RenameSlotName(const FName& OldName, const FName& NewName);

	////////////////////////////////////////////////////////////////////////////
	// Smart Name Interfaces
	////////////////////////////////////////////////////////////////////////////
	// Adds a new name to the smart name container and modifies the skeleton so it can be saved
	// return bool - Whether a name was added (false if already present)
#if WITH_EDITOR
	ENGINE_API bool AddSmartNameAndModify(FName ContainerName, FName NewDisplayName, FSmartName& NewName);

	// Renames a smartname in the specified container and modifies the skeleton
	// return bool - Whether the rename was sucessful
	ENGINE_API bool RenameSmartnameAndModify(FName ContainerName, SmartName::UID_Type Uid, FName NewName);

	// Removes a smartname from the specified container and modifies the skeleton
	ENGINE_API void RemoveSmartnameAndModify(FName ContainerName, SmartName::UID_Type Uid);

	// Removes smartnames from the specified container and modifies the skeleton
	ENGINE_API void RemoveSmartnamesAndModify(FName ContainerName, const TArray<FName>& Names);
#endif// WITH_EDITOR

	// quick wrapper function for Find UID by name, if not found, it will return SmartName::MaxUID
	ENGINE_API SmartName::UID_Type GetUIDByName(const FName& ContainerName, const FName& Name) const;
	ENGINE_API bool GetSmartNameByUID(const FName& ContainerName, SmartName::UID_Type UID, FSmartName& OutSmartName);
	ENGINE_API bool GetSmartNameByName(const FName& ContainerName, const FName& InName, FSmartName& OutSmartName);

	// Get or add a smartname container with the given name
	ENGINE_API const FSmartNameMapping* GetSmartNameContainer(const FName& ContainerName) const;

	// make sure the smart name has valid UID and so on
	ENGINE_API void VerifySmartName(const FName&  ContainerName, FSmartName& InOutSmartName);
	ENGINE_API void VerifySmartNames(const FName&  ContainerName, TArray<FSmartName>& InOutSmartNames);
private:
	// Get or add a smartname container with the given name
	FSmartNameMapping* GetOrAddSmartNameContainer(const FName& ContainerName);
	bool VerifySmartNameInternal(const FName&  ContainerName, FSmartName& InOutSmartName);
	bool FillSmartNameByDisplayName(FSmartNameMapping* Mapping, const FName& DisplayName, FSmartName& OutSmartName);
#if WITH_EDITORONLY_DATA
private:
	/** The default skeletal mesh to use when previewing this skeleton */
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TSoftObjectPtr<class USkeletalMesh> PreviewSkeletalMesh;

	/** The additional skeletal meshes to use when previewing this skeleton */
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TSoftObjectPtr<class UDataAsset> AdditionalPreviewSkeletalMeshes;

	UPROPERTY()
	FRigConfiguration RigConfig;

	/** rig property will be saved separately */
	ENGINE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

public:

	/** AnimNotifiers that has been created. Right now there is no delete step for this, but in the future we'll supply delete**/
	UPROPERTY()
	TArray<FName> AnimationNotifies;

	/* Attached assets component for this skeleton */
	UPROPERTY()
	FPreviewAssetAttachContainer PreviewAttachedAssetContainer;
#endif // WITH_EDITORONLY_DATA

private:
	DECLARE_MULTICAST_DELEGATE( FOnRetargetSourceChangedMulticaster )
	FOnRetargetSourceChangedMulticaster OnRetargetSourceChanged;

public:
	typedef FOnRetargetSourceChangedMulticaster::FDelegate FOnRetargetSourceChanged;

	/** Registers a delegate to be called after the preview animation has been changed */
	FDelegateHandle RegisterOnRetargetSourceChanged(const FOnRetargetSourceChanged& Delegate)
	{
		return OnRetargetSourceChanged.Add(Delegate);
	}

	const FGuid GetGuid() const
	{
		return Guid;
	}

	FGuid GetVirtualBoneGuid() const
	{
		return VirtualBoneGuid;
	}

	/** Unregisters a delegate to be called after the preview animation has been changed */
	void UnregisterOnRetargetSourceChanged(FDelegateHandle Handle)
	{
		OnRetargetSourceChanged.Remove(Handle);
	}

	void CallbackRetargetSourceChanged()
	{
		OnRetargetSourceChanged.Broadcast();
	}


	typedef TArray<FBoneNode> FBoneTreeType;

	/** Non-serialised cache of linkups between different skeletal meshes and this Skeleton. */
	TArray<struct FSkeletonToMeshLinkup> LinkupCache;

	/** Runtime built mapping table between SkeletalMeshes, and LinkupCache array indices. */
	TMap<TWeakObjectPtr<USkeletalMesh>, int32> SkelMesh2LinkupCache;

#if WITH_EDITORONLY_DATA

	// @todo document
	ENGINE_API void CollectAnimationNotifies();

	// @todo document
	ENGINE_API void AddNewAnimationNotify(FName NewAnimNotifyName);

	/** Returns the skeletons preview mesh, loading it if necessary */
	ENGINE_API USkeletalMesh* GetPreviewMesh(bool bFindIfNotSet=false);
	ENGINE_API USkeletalMesh* GetPreviewMesh() const;
	ENGINE_API USkeletalMesh* GetAssetPreviewMesh(UObject* InAsset);

	/** Find the first compatible mesh for this skeleton */
	ENGINE_API USkeletalMesh* FindCompatibleMesh() const;

	/** Returns the skeletons preview mesh, loading it if necessary */
	ENGINE_API void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty=true);

	/** Load any additional meshes we may have */
	ENGINE_API void LoadAdditionalPreviewSkeletalMeshes();

	/** Get the additional skeletal meshes we use when previewing this skeleton */
	ENGINE_API UDataAsset* GetAdditionalPreviewSkeletalMeshes() const;

	/** Set the additional skeletal meshes we use when previewing this skeleton */
	ENGINE_API void SetAdditionalPreviewSkeletalMeshes(UDataAsset* InPreviewCollectionAsset);

	/**
	 * Makes sure all attached objects are valid and removes any that aren't.
	 *
	 * @return		NumberOfBrokenAssets
	 */
	ENGINE_API int32 ValidatePreviewAttachedObjects();

	/**
	 * Get List of Child Bones of the ParentBoneIndex
	 *
	 * @param	Parent Bone Index
	 * @param	(out) List of Direct Children
	 */
	ENGINE_API int32 GetChildBones(int32 ParentBoneIndex, TArray<int32> & Children) const;

#endif

	/**
	 *	Check if this skeleton may be used with other skeleton
	 */
	ENGINE_API bool IsCompatible(USkeleton const * InSkeleton) const { return (InSkeleton && this == InSkeleton); }

	/** 
	 * Indexing naming convention
	 * 
	 * Since this code has indexing to very two distinct array but it can be confusing so I am making it consistency for naming
	 * 
	 * First index is SkeletalMesh->RefSkeleton index - I call this RefBoneIndex
	 * Second index is BoneTree index in USkeleton - I call this TreeBoneIndex
	 */

	/**
	 * Verify to see if we can match this skeleton with the provided SkeletalMesh.
	 * 
	 * Returns true 
	 *		- if bone hierarchy matches (at least needs to have matching parent) 
	 *		- and if parent chain matches - meaning if bone tree has A->B->C and if ref pose has A->C, it will fail
	 *		- and if more than 50 % of bones matches
	 *  
	 * @param	InSkelMesh	SkeletalMesh to compare the Skeleton against.
	 * 
	 * @return				true if animation set can play on supplied SkeletalMesh, false if not.
	 */
	ENGINE_API bool IsCompatibleMesh(const USkeletalMesh* InSkelMesh) const;

	/** Clears all cache data **/
	ENGINE_API void ClearCacheData();

	/** 
	 * Find a mesh linkup table (mapping of skeleton bone tree indices to refpose indices) for a particular SkeletalMesh
	 * If one does not already exist, create it now.
	 */
	ENGINE_API int32 GetMeshLinkupIndex(const USkeletalMesh* InSkelMesh);

	/** 
	 * Merge Bones (RequiredBones from InSkelMesh) to BoneTrees if not exists
	 * 
	 * Note that this bonetree can't ever clear up because doing so will corrupt all animation data that was imported based on this
	 * If nothing exists, it will build new bone tree 
	 * 
	 * @param InSkelMesh			: Mesh to build from. 
	 * @param RequiredRefBones		: RequiredBones are subset of list of bones (index to InSkelMesh->RefSkeleton)
									Most of cases, you don't like to add all bones to skeleton, so you'll have choice of cull out some
	 * 
	 * @return true if success
	 */
	ENGINE_API bool MergeBonesToBoneTree(const USkeletalMesh* InSkeletalMesh, const TArray<int32> &RequiredRefBones);

	/** 
	 * Merge all Bones to BoneTrees if not exists
	 * 
	 * Note that this bonetree can't ever clear up because doing so will corrupt all animation data that was imported based on this
	 * If nothing exists, it will build new bone tree 
	 * 
	 * @param InSkelMesh			: Mesh to build from. 
	 * 
	 * @return true if success
	 */
	ENGINE_API bool MergeAllBonesToBoneTree(const USkeletalMesh* InSkelMesh);

	/** 
	 * Merge has failed, then Recreate BoneTree
	 * 
	 * This will invalidate all animations that were linked before, but this is needed 
	 * 
	 * @param InSkelMesh			: Mesh to build from. 
	 * 
	 * @return true if success
	 */
	ENGINE_API bool RecreateBoneTree(USkeletalMesh* InSkelMesh);

	/** This is const accessor for BoneTree
	 *  Understand there will be a lot of need to access BoneTree, but 
	 *	Anybody modifying BoneTree will corrupt animation data, so will need to make sure it's not modifiable outside of Skeleton
	 *	You can add new BoneNode but you can't modify current list. The index will be referenced by Animation data.
	 */
	DEPRECATED(4.14, "GetBoneTree should not be called. Please use GetBoneTranslationRetargetingMode()/SetBoneTranslationRetargetingMode() functions instead")
	const TArray<FBoneNode>& GetBoneTree()	const
	{ 
		return BoneTree;	
	}

	// @todo document
	const TArray<FTransform>& GetRefLocalPoses( FName RetargetSource = NAME_None ) const 
	{
		if ( RetargetSource != NAME_None ) 
		{
			const FReferencePose* FoundRetargetSource = AnimRetargetSources.Find(RetargetSource);
			if (FoundRetargetSource)
			{
				return FoundRetargetSource->ReferencePose;
			}
		}

		return ReferenceSkeleton.GetRefBonePose();	
	}

	/** 
	 * Get Track index of InAnimSeq for the BoneTreeIndex of BoneTree
	 * this is slow, and it's not supposed to be used heavily
	 * @param	InBoneTreeIdx	BoneTree Index
	 * @param	InAnimSeq		Animation Sequence to get track index for 
	 *
	 * @return	Index of Track of Animation Sequence
	 */
	ENGINE_API int32 GetAnimationTrackIndex(const int32 InSkeletonBoneIndex, const UAnimSequence* InAnimSeq, const bool bUseRawData);

	/** 
	 * Get Bone Tree Index from Reference Bone Index
	 * @param	InSkelMesh	SkeletalMesh for the ref bone idx
	 * @param	InRefBoneIdx	Reference Bone Index to look for - index of USkeletalMesh.RefSkeleton
	 * @return	Index of BoneTree Index
	 */
	ENGINE_API int32 GetSkeletonBoneIndexFromMeshBoneIndex(const USkeletalMesh* InSkelMesh, const int32 MeshBoneIndex);

	/** 
	 * Get Reference Bone Index from Bone Tree Index
	 * @param	InSkelMesh	SkeletalMesh for the ref bone idx
	 * @param	InBoneTreeIdx	Bone Tree Index to look for - index of USkeleton.BoneTree
	 * @return	Index of BoneTree Index
	 */
	ENGINE_API int32 GetMeshBoneIndexFromSkeletonBoneIndex(const USkeletalMesh* InSkelMesh, const int32 SkeletonBoneIndex);

	EBoneTranslationRetargetingMode::Type GetBoneTranslationRetargetingMode(const int32 BoneTreeIdx) const
	{
		if (BoneTree.IsValidIndex(BoneTreeIdx))
		{
			return BoneTree[BoneTreeIdx].TranslationRetargetingMode;
		}
		return EBoneTranslationRetargetingMode::Animation;
	}

	/** 
	 * Rebuild Look up between SkelMesh to BoneTree - this should only get called when SkelMesh is re-imported or so, where the mapping may be no longer valid
	 *
	 * @param	InSkelMesh	: SkeletalMesh to build look up for
	 */
	void RebuildLinkup(const USkeletalMesh* InSkelMesh);

	/**
	 * Remove Link up cache for the SkelMesh
	 *
	 * @param	InSkelMesh	: SkeletalMesh to remove linkup cache for 
	 */
	void RemoveLinkup(const USkeletalMesh* InSkelMesh);

	ENGINE_API void SetBoneTranslationRetargetingMode(const int32 BoneIndex, EBoneTranslationRetargetingMode::Type NewRetargetingMode, bool bChildrenToo=false);

	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
	ENGINE_API virtual void PostInitProperties() override;
	ENGINE_API virtual void Serialize(FArchive& Ar) override;

	ENGINE_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/** 
	 * Create RefLocalPoses from InSkelMesh. Note InSkelMesh cannot be null and this function will assert if it is.
	 * 
	 * If bClearAll is false, it will overwrite ref pose of bones that are found in InSkelMesh
	 * If bClearAll is true, it will reset all Reference Poses 
	 * Note that this means it will remove transforms of extra bones that might not be found in this skeletalmesh
	 *
	 * @return true if successful. false if skeletalmesh wasn't compatible with the bone hierarchy
	 */
	ENGINE_API void UpdateReferencePoseFromMesh(const USkeletalMesh* InSkelMesh);

#if WITH_EDITORONLY_DATA
	/**
	 * Update Retarget Source with given name
	 *
	 * @param Name	Name of pose to update
	 */
	ENGINE_API void UpdateRetargetSource( const FName InName );
#endif
protected:
	/** 
	 * Check if Parent Chain Matches between BoneTree, and SkelMesh 
	 * Meaning if BoneTree has A->B->C (top to bottom) and if SkelMesh has A->C
	 * It will fail since it's missing B
	 * We ensure this chain matches to play animation properly
	 *
	 * @param StartBoneIndex	: BoneTreeIndex to start from in BoneTree 
	 * @param InSkelMesh		: SkeletalMesh to compare
	 *
	 * @return true if matches till root. false if not. 
	 */
	bool DoesParentChainMatch(int32 StartBoneTreeIndex, const USkeletalMesh* InSkelMesh) const;

	/** 
	 * Build Look up between SkelMesh to BoneTree
	 *
	 * @param	InSkelMesh	: SkeletalMesh to build look up for
	 * @return	Index of LinkupCache that this SkelMesh is linked to 
	 */
	int32 BuildLinkup(const USkeletalMesh* InSkelMesh);

#if WITH_EDITORONLY_DATA
	/**
	 * Refresh All Retarget Sources
	 */
	void RefreshAllRetargetSources();
#endif
	/**
	 * Create Reference Skeleton From the given Mesh 
	 * 
	 * @param InSkeletonMesh	SkeletalMesh that this Skeleton is based on
	 * @param RequiredRefBones	List of required bones to create skeleton from
	 *
	 * @return true if successful
	 */
	bool CreateReferenceSkeletonFromMesh(const USkeletalMesh* InSkeletalMesh, const TArray<int32> & RequiredRefBones);

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE( FOnSkeletonHierarchyChangedMulticaster );
	FOnSkeletonHierarchyChangedMulticaster OnSkeletonHierarchyChanged;

	/** Call this when the skeleton has changed to fix dependent assets */
	ENGINE_API void HandleSkeletonHierarchyChange();

public:
	typedef FOnSkeletonHierarchyChangedMulticaster::FDelegate FOnSkeletonHierarchyChanged;

	/** Registers a delegate to be called after notification has changed*/
	ENGINE_API void RegisterOnSkeletonHierarchyChanged(const FOnSkeletonHierarchyChanged& Delegate);
	ENGINE_API void UnregisterOnSkeletonHierarchyChanged(void* Unregister);

	/** Removes the supplied bones from the skeleton */
	ENGINE_API void RemoveBonesFromSkeleton(const TArray<FName>& BonesToRemove, bool bRemoveChildBones);

	// Asset registry information for animation notifies
	static const FName AnimNotifyTag;
	static const FString AnimNotifyTagDelimiter;

	// Asset registry information for animation curves
	ENGINE_API static const FName CurveNameTag;
	ENGINE_API static const FString CurveTagDelimiter;

	// rig Configs
	ENGINE_API static const FName RigTag;
	ENGINE_API void SetRigConfig(URig * Rig);
	ENGINE_API FName GetRigBoneMapping(const FName& NodeName) const;
	ENGINE_API bool SetRigBoneMapping(const FName& NodeName, FName BoneName);
	ENGINE_API FName GetRigNodeNameFromBoneName(const FName& BoneName) const;
	// this make sure it stays within the valid range
	ENGINE_API int32 GetMappedValidNodes(TArray<FName> &OutValidNodeNames);
	// verify if it has all latest data
	ENGINE_API void RefreshRigConfig();
	int32 FindRigBoneMapping(const FName& NodeName) const;
	ENGINE_API URig * GetRig() const;

#endif

public:
	ENGINE_API USkeletalMeshSocket* FindSocketAndIndex(FName InSocketName, int32& OutIndex) const;
	ENGINE_API USkeletalMeshSocket* FindSocket(FName InSocketName) const;

private:
	/** Regenerate new Guid */
	void RegenerateGuid();
	void RegenerateVirtualBoneGuid();




public:
	//~ Begin IInterface_AssetUserData Interface
	ENGINE_API virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	ENGINE_API virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface
protected:
	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = Skeleton)
	TArray<UAssetUserData*> AssetUserData;
};

