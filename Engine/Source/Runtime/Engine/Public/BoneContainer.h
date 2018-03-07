// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "BoneIndices.h"
#include "ReferenceSkeleton.h"
#include "Animation/AnimTypes.h"
#include "BoneContainer.generated.h"

class USkeletalMesh;
class USkeleton;
class USkeletalMesh;

/**
* This is a native transient structure. Used to store virtual bone mappings for compact poses
**/
struct FVirtualBoneCompactPoseData
{
	/** Index of this virtual bone */
	FCompactPoseBoneIndex VBIndex;
	/** Index of source bone */
	FCompactPoseBoneIndex SourceIndex;
	/** Index of target bone */
	FCompactPoseBoneIndex TargetIndex;

	FVirtualBoneCompactPoseData(FCompactPoseBoneIndex InVBIndex, FCompactPoseBoneIndex InSourceIndex, FCompactPoseBoneIndex InTargetIndex)
		: VBIndex(InVBIndex)
		, SourceIndex(InSourceIndex)
		, TargetIndex(InTargetIndex)
	{}
};

/**
 * This is curve evaluation options for bone container
 */
struct FCurveEvaluationOption
{
	bool bAllowCurveEvaluation;
	const TArray<FName>* DisallowedList;
	int32 LODIndex;
	
	FCurveEvaluationOption(bool bInAllowCurveEvaluation = true, const TArray<FName>* InDisallowedList = nullptr, int32 InLODIndex = 0)
		: bAllowCurveEvaluation(bInAllowCurveEvaluation)
		, DisallowedList(InDisallowedList)
		, LODIndex(InLODIndex)
	{
	}
};
/**
* This is a native transient structure.
* Contains:
* - BoneIndicesArray: Array of RequiredBoneIndices for Current Asset. In increasing order. Mapping to current Array of Transforms (Pose).
* - BoneSwitchArray: Size of current Skeleton. true if Bone is contained in RequiredBones array, false otherwise.
**/
struct ENGINE_API FBoneContainer
{
private:
	/** Array of RequiredBonesIndices. In increasing order. */
	TArray<FBoneIndexType>	BoneIndicesArray;
	/** Array sized by Current RefPose. true if Bone is contained in RequiredBones array, false otherwise. */
	TBitArray<>				BoneSwitchArray;

	/** Asset BoneIndicesArray was made for. Typically a SkeletalMesh. */
	TWeakObjectPtr<UObject>	Asset;
	/** If Asset is a SkeletalMesh, this will be a pointer to it. Can be NULL if Asset is a USkeleton. */
	TWeakObjectPtr<USkeletalMesh> AssetSkeletalMesh;
	/** If Asset is a Skeleton that will be it. If Asset is a SkeletalMesh, that will be its Skeleton. */
	TWeakObjectPtr<USkeleton> AssetSkeleton;

	/** Pointer to RefSkeleton of Asset. */
	const FReferenceSkeleton* RefSkeleton;

	/** Mapping table between Skeleton Bone Indices and Pose Bone Indices. */
	TArray<int32> SkeletonToPoseBoneIndexArray;

	/** Mapping table between Pose Bone Indices and Skeleton Bone Indices. */
	TArray<int32> PoseToSkeletonBoneIndexArray;

	// Look up from skeleton to compact pose format
	TArray<int32> CompactPoseToSkeletonIndex;

	// Look up from compact pose format to skeleton
	TArray<FCompactPoseBoneIndex> SkeletonToCompactPose;

	/** Animation Curve UID array that matters to this container. Recalculated everytime LOD changes. */
	TArray<SmartName::UID_Type> AnimCurveNameUids;

	// Compact pose format of Parent Bones (to save us converting to mesh space and back)
	TArray<FCompactPoseBoneIndex> CompactPoseParentBones;

	// Compact pose format of Ref Pose Bones (to save us converting to mesh space and back)
	TArray<FTransform>    CompactPoseRefPoseBones;

	// Array of cached virtual bone data so that animations running from raw data can generate them.
	TArray<FVirtualBoneCompactPoseData> VirtualBoneCompactPoseData;

	/** For debugging. */
	/** Disable Retargeting. Extract animation, but do not retarget it. */
	bool bDisableRetargeting;
	/** Disable animation compression, use RAW data instead. */
	bool bUseRAWData;
	/** Use Source Data that is imported that are not compressed. */
	bool bUseSourceData;

public:

	FBoneContainer();

	FBoneContainer(const TArray<FBoneIndexType>& InRequiredBoneIndexArray, const FCurveEvaluationOption& CurveEvalOption, UObject& InAsset);

	/** Initialize BoneContainer to a new Asset, RequiredBonesArray and RefPoseArray. */
	void InitializeTo(const TArray<FBoneIndexType>& InRequiredBoneIndexArray, const FCurveEvaluationOption& CurveEvalOption, UObject& InAsset);

	/** Returns true if FBoneContainer is Valid. Needs an Asset, a RefPoseArray, and a RequiredBonesArray. */
	const bool IsValid() const
	{
		return (Asset.IsValid() && (RefSkeleton != NULL) && (BoneIndicesArray.Num() > 0));
	}

	/** Get Asset this BoneContainer was made for. Typically a SkeletalMesh, but could also be a USkeleton. */
	UObject* GetAsset() const
	{
		return Asset.Get();
	}

	/** Get SkeletalMesh Asset this BoneContainer was made for. Could be NULL if Asset is a Skeleton. */
	USkeletalMesh* GetSkeletalMeshAsset() const
	{
		return AssetSkeletalMesh.Get();
	}

	/** Get Skeleton Asset. Could either be the SkeletalMesh's Skeleton, or the Skeleton this BoneContainer was made for. Is non NULL is BoneContainer is valid. */
	USkeleton* GetSkeletonAsset() const
	{
		return AssetSkeleton.Get();
	}

	/** Disable Retargeting for debugging. */
	void SetDisableRetargeting(bool InbDisableRetargeting)
	{
		bDisableRetargeting = InbDisableRetargeting;
	}

	/** True if retargeting is disabled for debugging. */
	bool GetDisableRetargeting() const
	{
		return bDisableRetargeting;
	}

	/** Ignore compressed data and use RAW data instead, for debugging. */
	void SetUseRAWData(bool InbUseRAWData)
	{
		bUseRAWData = InbUseRAWData;
	}

	/** True if we're requesting RAW data instead of compressed data. For debugging. */
	bool ShouldUseRawData() const
	{
		return bUseRAWData;
	}

	/** use Source data instead.*/
	void SetUseSourceData(bool InbUseSourceData)
	{
		bUseSourceData = InbUseSourceData;
	}

	/** True if we're requesting Source data instead of RawAnimationData. For debugging. */
	bool ShouldUseSourceData() const
	{
		return bUseSourceData;
	}

	/**
	* returns Required Bone Indices Array
	*/
	const TArray<FBoneIndexType>& GetBoneIndicesArray() const
	{
		return BoneIndicesArray;
	}

	/**
	* returns virutal bone cached data
	*/
	const TArray<FVirtualBoneCompactPoseData>& GetVirtualBoneCompactPoseData() const { return VirtualBoneCompactPoseData; }

	/**
	* returns Bone Switch Array. BitMask for RequiredBoneIndex array.
	*/
	const TBitArray<>& GetBoneSwitchArray() const
	{
		return BoneSwitchArray;
	}

	/** Pointer to RefPoseArray for current Asset. */
	const TArray<FTransform>& GetRefPoseArray() const
	{
		return RefSkeleton->GetRefBonePose();
	}

	const FTransform& GetRefPoseTransform(const FCompactPoseBoneIndex& BoneIndex) const
	{
		return CompactPoseRefPoseBones[BoneIndex.GetInt()];
	}

	const TArray<FTransform>& GetRefPoseCompactArray() const
	{
		return CompactPoseRefPoseBones;
	}

	void SetRefPoseCompactArray(const TArray<FTransform>& InRefPoseCompactArray)
	{
		check(InRefPoseCompactArray.Num() == CompactPoseRefPoseBones.Num());
		CompactPoseRefPoseBones = InRefPoseCompactArray;
	}

	/** Access to Asset's RefSkeleton. */
	const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return *RefSkeleton;
	}

	/** Number of Bones in RefPose for current asset. This is NOT the number of bones in RequiredBonesArray, but the TOTAL number of bones in the RefPose of the current Asset! */
	const int32 GetNumBones() const
	{
		return RefSkeleton->GetNum();
	}

	const int32 GetCompactPoseNumBones() const
	{
		return BoneIndicesArray.Num();
	}

	/** Get BoneIndex for BoneName for current Asset. */
	int32 GetPoseBoneIndexForBoneName(const FName& BoneName) const;

	/** Get ParentBoneIndex for current Asset. */
	int32 GetParentBoneIndex(const int32 BoneIndex) const;

	/** Get ParentBoneIndex for current Asset. */
	FCompactPoseBoneIndex GetParentBoneIndex(const FCompactPoseBoneIndex& BoneIndex) const;

	/** Get Depth between bones for current asset. */
	int32 GetDepthBetweenBones(const int32 BoneIndex, const int32 ParentBoneIndex) const;

	/** Returns true if bone is child of for current asset. */
	bool BoneIsChildOf(const int32 BoneIndex, const int32 ParentBoneIndex) const;

	/** Returns true if bone is child of for current asset. */
	bool BoneIsChildOf(const FCompactPoseBoneIndex& BoneIndex, const FCompactPoseBoneIndex& ParentBoneIndex) const;

	/** Get AnimCurveNameUids for curve evaluation */
	TArray<SmartName::UID_Type> const& GetAnimCurveNameUids() const
	{
		return AnimCurveNameUids; 
	} 

	/**
	* Serializes the bones
	*
	* @param Ar - The archive to serialize into.
	* @param Rect - The bone container to serialize.
	*
	* @return Reference to the Archive after serialization.
	*/
	friend FArchive& operator<<(FArchive& Ar, FBoneContainer& B)
	{
		Ar
			<< B.BoneIndicesArray
			<< B.BoneSwitchArray
			<< B.Asset
			<< B.AssetSkeletalMesh
			<< B.AssetSkeleton
			<< B.SkeletonToPoseBoneIndexArray
			<< B.PoseToSkeletonBoneIndexArray
			<< B.bDisableRetargeting
			<< B.bUseRAWData
			<< B.bUseSourceData
			;

		return Ar;
	}

	/**
	* Returns true of RequiredBonesArray contains this bone index.
	*/
	bool Contains(FBoneIndexType NewIndex) const
	{
		return BoneSwitchArray[NewIndex];
	}

	/** Const accessor to GetSkeletonToPoseBoneIndexArray(). */
	TArray<int32> const & GetSkeletonToPoseBoneIndexArray() const
	{
		return SkeletonToPoseBoneIndexArray;
	}

	/** Const accessor to GetSkeletonToPoseBoneIndexArray(). */
	TArray<int32> const & GetPoseToSkeletonBoneIndexArray() const
	{
		return PoseToSkeletonBoneIndexArray;
	}

	int32 GetSkeletonIndex(const FCompactPoseBoneIndex& BoneIndex) const
	{
		return CompactPoseToSkeletonIndex[BoneIndex.GetInt()];
	}

	FCompactPoseBoneIndex GetCompactPoseIndexFromSkeletonIndex(const int32 SkeletonIndex) const
	{
		return SkeletonToCompactPose[SkeletonIndex];
	}

	FMeshPoseBoneIndex MakeMeshPoseIndex(const FCompactPoseBoneIndex& BoneIndex) const
	{
		return FMeshPoseBoneIndex(GetBoneIndicesArray()[BoneIndex.GetInt()]);
	}

	FCompactPoseBoneIndex MakeCompactPoseIndex(const FMeshPoseBoneIndex& BoneIndex) const
	{
		return FCompactPoseBoneIndex(GetBoneIndicesArray().IndexOfByKey(BoneIndex.GetInt()));
	}

	/** Cache required Anim Curve Uids */
	void CacheRequiredAnimCurveUids(const FCurveEvaluationOption& CurveEvalOption);

private:
	/** Initialize FBoneContainer. */
	void Initialize(const FCurveEvaluationOption& CurveEvalOption);

	/** Cache remapping data if current Asset is a SkeletalMesh, with all compatible Skeletons. */
	void RemapFromSkelMesh(USkeletalMesh const & SourceSkeletalMesh, USkeleton& TargetSkeleton);

	/** Cache remapping data if current Asset is a Skeleton, with all compatible Skeletons. */
	void RemapFromSkeleton(USkeleton const & SourceSkeleton);
};
USTRUCT()
struct FBoneReference
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, Category = BoneReference)
	FName BoneName;

	/** Cached bone index for run time - right now bone index of skeleton **/
	int32 BoneIndex;

	/** Change this to Bitfield if we have more than one bool 
	 * This specifies whether or not this indices is mesh or skeleton
	 */
	bool bUseSkeletonIndex;

	FCompactPoseBoneIndex CachedCompactPoseIndex;

	FBoneReference()
		: BoneIndex(INDEX_NONE)
		, bUseSkeletonIndex(false)
		, CachedCompactPoseIndex(INDEX_NONE)
	{
	}

	FBoneReference(const FName& InBoneName)
		: BoneName(InBoneName)
		, BoneIndex(INDEX_NONE)
		, bUseSkeletonIndex(false)
		, CachedCompactPoseIndex(INDEX_NONE)
	{
	}

	bool operator==(const FBoneReference& Other) const
	{
		// faster to compare, and BoneName won't matter
		return BoneIndex == Other.BoneIndex;
	}
	/** Initialize Bone Reference, return TRUE if success, otherwise, return false **/
	ENGINE_API bool Initialize(const FBoneContainer& RequiredBones);

	// only used by blendspace 'PerBoneBlend'. This is skeleton indices since the input is skeleton
	// @note, if you use this function, it won't work with GetCompactPoseIndex, GetMeshPoseIndex;
	// it triggers ensure in those functions
	ENGINE_API bool Initialize(const USkeleton* Skeleton);

	/** Deprecated functions */
	DEPRECATED(4.17, "Please use IsValidToEvaluate instead")
	ENGINE_API bool IsValid(const FBoneContainer& RequiredBones) const;
	
	/** return true if it has valid set up */
	bool HasValidSetup() const
	{
		return (BoneIndex != INDEX_NONE);
	}

	/** return true if has valid index, and required bones contain it **/
	ENGINE_API bool IsValidToEvaluate(const FBoneContainer& RequiredBones) const;
	/** return true if has valid compact index. This will return invalid if you're using skeleton index */
	ENGINE_API bool IsValidToEvaluate() const
	{
		return (!bUseSkeletonIndex && CachedCompactPoseIndex != INDEX_NONE);
	}

	FMeshPoseBoneIndex GetMeshPoseIndex(const FBoneContainer& RequiredBones) const
	{ 
		// accessing array with invalid index would cause crash, so we have to check here
		if (BoneIndex != INDEX_NONE)
		{
			if (bUseSkeletonIndex)
			{
				return FMeshPoseBoneIndex(RequiredBones.GetSkeletonToPoseBoneIndexArray()[BoneIndex]);
			}
			else
			{
				return FMeshPoseBoneIndex(BoneIndex);
			}
		}

		return FMeshPoseBoneIndex(INDEX_NONE);
	}

	FCompactPoseBoneIndex GetCompactPoseIndex(const FBoneContainer& RequiredBones) const 
	{ 
		if (bUseSkeletonIndex)
		{
			//If we were initialized with a skeleton we wont have a cached index.
			if (BoneIndex != INDEX_NONE)
			{
				// accessing array with invalid index would cause crash, so we have to check here
				return RequiredBones.GetCompactPoseIndexFromSkeletonIndex(BoneIndex);
			}
			return FCompactPoseBoneIndex(INDEX_NONE);
		}
		
		return CachedCompactPoseIndex;
	}

	// need this because of BoneReference being used in CurveMetaData and that is in SmartName
	friend FArchive& operator<<(FArchive& Ar, FBoneReference& B)
	{
		Ar << B.BoneName;
		return Ar;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}
};
