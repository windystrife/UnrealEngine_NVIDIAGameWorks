// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "BoneIndices.h"
#include "Animation/AnimTypes.h"
#include "CustomBoneIndexArray.h"
#include "AnimEncoding.h"
#include "Animation/AnimStats.h"

struct FBoneTransform
{
	/** @todo anim: should be Skeleton bone index in the future, but right now it's CompactBoneIndex **/
	FCompactPoseBoneIndex BoneIndex;

	/** Transform to apply **/
	FTransform Transform;

	FBoneTransform()
		: BoneIndex(INDEX_NONE)
	{}

	FBoneTransform(FCompactPoseBoneIndex InBoneIndex, const FTransform& InTransform)
		: BoneIndex(InBoneIndex)
		, Transform(InTransform)
	{}
};

// Comparison Operator for Sorting.
struct FCompareBoneTransformIndex
{
	FORCEINLINE bool operator()(const FBoneTransform& A, const FBoneTransform& B) const
	{
		return A.BoneIndex < B.BoneIndex;
	}
};


template<class BoneIndexType, typename InAllocator>
struct FBasePose
{
public:
	FORCEINLINE void InitBones(int32 NumBones) { Bones.Reset(NumBones); Bones.AddUninitialized(NumBones); }

	FORCEINLINE int32 GetNumBones() const { return Bones.Num(); }

	FORCEINLINE bool IsValidIndex(const BoneIndexType& BoneIndex) const
	{
		return Bones.IsValidIndex(BoneIndex.GetInt());
	}

	FORCEINLINE FTransform& operator[](const BoneIndexType& BoneIndex)
	{ 
		return Bones[BoneIndex.GetInt()];
	}

	FORCEINLINE const FTransform& operator[] (const BoneIndexType& BoneIndex) const
	{
		return Bones[BoneIndex.GetInt()];
	}

	//Bone Index Iteration
	template<typename PoseType, typename IterType>
	struct FRangedForSupport
	{
		const PoseType& Pose;

		FRangedForSupport(const PoseType& InPose) : Pose(InPose) {};
		
		IterType begin() { return Pose.MakeBeginIter(); }
		IterType end() { return Pose.MakeEndIter(); }
	};

	template<typename PoseType, typename IterType>
	struct FRangedForReverseSupport
	{
		const PoseType& Pose;

		FRangedForReverseSupport(const PoseType& InPose) : Pose(InPose) {};

		IterType begin() { return Pose.MakeBeginIterReverse(); }
		IterType end() { return Pose.MakeEndIterReverse(); }
	};

	const TArray<FTransform, InAllocator>& GetBones() const { return Bones; }
protected:
	TArray<FTransform, InAllocator> Bones;
};

struct FCompactPoseBoneIndexIterator
{
	int32 Index;

	FCompactPoseBoneIndexIterator(int32 InIndex) : Index(InIndex) {}

	FCompactPoseBoneIndexIterator& operator++() { ++Index; return (*this); }
	bool operator==(FCompactPoseBoneIndexIterator& Rhs) { return Index == Rhs.Index; }
	bool operator!=(FCompactPoseBoneIndexIterator& Rhs) { return Index != Rhs.Index; }
	FCompactPoseBoneIndex operator*() const { return FCompactPoseBoneIndex(Index); }
};

struct FCompactPoseBoneIndexReverseIterator
{
	int32 Index;

	FCompactPoseBoneIndexReverseIterator(int32 InIndex) : Index(InIndex) {}

	FCompactPoseBoneIndexReverseIterator& operator++() { --Index; return (*this); }
	bool operator==(FCompactPoseBoneIndexReverseIterator& Rhs) { return Index == Rhs.Index; }
	bool operator!=(FCompactPoseBoneIndexReverseIterator& Rhs) { return Index != Rhs.Index; }
	FCompactPoseBoneIndex operator*() const { return FCompactPoseBoneIndex(Index); }
};

template <typename InAllocator>
struct FBaseCompactPose : FBasePose<FCompactPoseBoneIndex, InAllocator>
{
public:

	FBaseCompactPose()
		: BoneContainer(nullptr)
	{}

	typedef FCompactPoseBoneIndex BoneIndexType;
	typedef InAllocator   Allocator;
	//--------------------------------------------------------------------------
	//Bone Index Iteration
	typedef typename FBasePose<FCompactPoseBoneIndex, Allocator>::template FRangedForSupport<FBaseCompactPose, FCompactPoseBoneIndexIterator> RangedForBoneIndexFwd;
	typedef typename FBasePose<FCompactPoseBoneIndex, Allocator>::template FRangedForReverseSupport<FBaseCompactPose, FCompactPoseBoneIndexReverseIterator> RangedForBoneIndexBwd;

	FORCEINLINE RangedForBoneIndexFwd ForEachBoneIndex() const
	{
		return RangedForBoneIndexFwd(*this);
	}

	FORCEINLINE RangedForBoneIndexBwd ForEachBoneIndexReverse() const
	{
		return RangedForBoneIndexBwd(*this);
	}

	FORCEINLINE FCompactPoseBoneIndexIterator MakeBeginIter() const { return FCompactPoseBoneIndexIterator(0); }

	FORCEINLINE FCompactPoseBoneIndexIterator MakeEndIter() const { return FCompactPoseBoneIndexIterator(this->GetNumBones()); }

	FORCEINLINE FCompactPoseBoneIndexReverseIterator MakeBeginIterReverse() const { return FCompactPoseBoneIndexReverseIterator(this->GetNumBones() - 1); }

	FORCEINLINE FCompactPoseBoneIndexReverseIterator MakeEndIterReverse() const { return FCompactPoseBoneIndexReverseIterator(-1); }
	//--------------------------------------------------------------------------

	const FBoneContainer& GetBoneContainer() const 
	{
		checkSlow(BoneContainer && BoneContainer->IsValid());
		return *BoneContainer;
	}

	void SetBoneContainer(const FBoneContainer* InBoneContainer)
	{
		check(InBoneContainer && InBoneContainer->IsValid());
		BoneContainer = InBoneContainer;
		this->InitBones(BoneContainer->GetBoneIndicesArray().Num());
	}

	void InitFrom(const FBaseCompactPose& SrcPose)
	{
		SetBoneContainer(SrcPose.BoneContainer);
		this->Bones = SrcPose.Bones;
	}

	// Copy bone transform from SrcPose to this
	template <typename OtherAllocator>
	void CopyBonesFrom(const FBaseCompactPose<OtherAllocator>& SrcPose)
	{
		this->Bones = SrcPose.GetBones();
		BoneContainer = &SrcPose.GetBoneContainer();
	}

	void CopyBonesFrom(const FBaseCompactPose<Allocator>& SrcPose)
	{
		if (this != &SrcPose)
		{
			this->Bones = SrcPose.GetBones();
			BoneContainer = &SrcPose.GetBoneContainer();
		}
	}
	
	template <typename OtherAllocator>
	void CopyBonesFrom(const TArray<FTransform, OtherAllocator>& SrcPoseBones)
	{
		// only allow if the size is same
		// if size doesn't match, we can't guarantee the bonecontainer would work
		// so we can't accept
		if (this->Bones.Num() == SrcPoseBones.Num())
		{
			this->Bones = SrcPoseBones;
		}
	}

	template <typename OtherAllocator>
	void CopyBonesTo(TArray<FTransform, OtherAllocator>& DestPoseBones)
	{
		// this won't work if you're copying to FBaseCompactPose without BoneContainer data
		// you'll like to make CopyBonesTo(FBaseCompactPose<OtherAllocator>& DestPose) to fix this properly
		// if you need bone container
		DestPoseBones = this->Bones;
	}

	// Sets this pose to its ref pose
	void ResetToRefPose()
	{
		ResetToRefPose(GetBoneContainer());
	}

	// Sets this pose to the supplied BoneContainers ref pose
	void ResetToRefPose(const FBoneContainer& RequiredBones)
	{
		const TArray<FTransform>& RefPoseCompactArray = RequiredBones.GetRefPoseCompactArray();
		this->Bones.Reset(RefPoseCompactArray.Num());
		this->Bones.Append(RefPoseCompactArray);

		// If retargeting is disabled, copy ref pose from Skeleton, rather than mesh.
		// this is only used in editor and for debugging.
		if (RequiredBones.GetDisableRetargeting())
		{
			checkSlow(RequiredBones.IsValid());
			// Only do this if we have a mesh. otherwise we're not retargeting animations.
			if (RequiredBones.GetSkeletalMeshAsset())
			{
				TArray<FTransform> const & SkeletonRefPose = RequiredBones.GetSkeletonAsset()->GetRefLocalPoses();

				for (const FCompactPoseBoneIndex BoneIndex : ForEachBoneIndex())
				{
					const int32 SkeletonBoneIndex = GetBoneContainer().GetSkeletonIndex(BoneIndex);

					// Pose bone index should always exist in Skeleton
					checkSlow(SkeletonBoneIndex != INDEX_NONE);
					this->Bones[BoneIndex.GetInt()] = SkeletonRefPose[SkeletonBoneIndex];
				}
			}
		}
	}

	// Sets every bone transform to Identity
	void ResetToAdditiveIdentity()
	{
		for (FTransform& Bone : this->Bones)
		{
			Bone.SetIdentity();
			Bone.SetScale3D(FVector::ZeroVector);
		}
	}

	// returns true if all bone rotations are normalized
	bool IsNormalized() const
	{
		for (const FTransform& Bone : this->Bones)
		{
			if (!Bone.IsRotationNormalized())
			{
				return false;
			}
		}

		return true;
	}

	// Returns true if any bone rotation contains NaN or Inf
	bool ContainsNaN() const
	{
		for (const FTransform& Bone : this->Bones)
		{
			if (Bone.ContainsNaN())
			{
				return true;
			}
		}

		return false;
	}


	// Normalizes all rotations in this pose
	void NormalizeRotations()
	{
		for (FTransform& Bone : this->Bones)
		{
			Bone.NormalizeRotation();
		}
	}

	bool IsValid() const
	{
		return (BoneContainer && BoneContainer->IsValid());
	}

	// Returns the bone index for the parent bone
	BoneIndexType GetParentBoneIndex(const BoneIndexType& BoneIndex) const
	{
		return GetBoneContainer().GetParentBoneIndex(BoneIndex);
	}

	// Returns the ref pose for the supplied bone
	const FTransform& GetRefPose(const BoneIndexType& BoneIndex) const
	{
		return GetBoneContainer().GetRefPoseTransform(BoneIndex);
	}

	// Populates this pose from the supplied animation and track data
	void PopulateFromAnimation(
		const UAnimSequence& Seq,
		const BoneTrackArray& RotationTracks,
		const BoneTrackArray& TranslationTracks,
		const BoneTrackArray& ScaleTracks,
		float Time)
	{
		// @todo fixme 
		FTransformArray LocalBones;
		LocalBones = this->Bones;

		AnimationFormat_GetAnimationPose(
			LocalBones, //@TODO:@ANIMATION: Nasty hack
			RotationTracks,
			TranslationTracks,
			ScaleTracks,
			Seq,
			Time);
		this->Bones = LocalBones;
	}

protected:

	//Reference to our BoneContainer
	const FBoneContainer* BoneContainer;
};

struct FCompactHeapPose;

struct ENGINE_API FCompactPose : public FBaseCompactPose<FAnimStackAllocator>
{
};

struct ENGINE_API FCompactHeapPose : public FBaseCompactPose<FDefaultAllocator>
{
	// Moves transform data out of the supplied SrcPose (SrcPose will be left empty)
	void MoveBonesFrom(FCompactHeapPose& SrcPose)
	{
		Bones = MoveTemp(SrcPose.Bones);
		BoneContainer = SrcPose.BoneContainer;
		SrcPose.BoneContainer = NULL;
	}

	//Moves transform data to supplied array (pose will be left empty)
	void MoveBonesTo(TArray<FTransform>& OutTransforms)
	{
		OutTransforms = MoveTemp(Bones);
		BoneContainer = NULL;
	}

	// Moves transform data out of the supplied InTransforms. InTransform will be left empty
	void MoveBonesFrom(TArray<FTransform>& InTransforms)
	{
		// if number doesn't match it is not compatible to start with
		if (InTransforms.Num() == Bones.Num())
		{
			Bones = MoveTemp(InTransforms);
		}
	}
};

struct FMeshPose : public FBasePose<FMeshPoseBoneIndex, FDefaultAllocator>
{
public:
	typedef FMeshPoseBoneIndex BoneIndexType;

	const FBoneContainer& GetBoneContainer() const
	{
		checkSlow(BoneContainer && BoneContainer->IsValid());
		return *BoneContainer;
	}

	void SetBoneContainer(const FBoneContainer* InBoneContainer)
	{
		check(InBoneContainer && InBoneContainer->IsValid());
		BoneContainer = InBoneContainer;
		InitBones(BoneContainer->GetNumBones());
	}

	//void InitFrom(FCompactPose& CompactPose);

	void MoveBonesTo(TArray<FTransform>& OutTransforms)
	{
		OutTransforms = MoveTemp(Bones);
		BoneContainer = NULL;
	}

	// Sets this pose to its ref pose
	ENGINE_API void ResetToRefPose();

	// Sets every bone transform to Identity
	void ResetToIdentity();

	// returns true if all bone rotations are normalized
	bool IsNormalized() const;

	// Returns true if any bone rotation contains NaN
	bool ContainsNaN() const;


	FORCEINLINE BoneIndexType GetParentBone(const BoneIndexType& BoneIndex)
	{
		return BoneIndexType(BoneContainer->GetParentBoneIndex(BoneIndex.GetInt()));
	}

protected:

	//Reference to our BoneContainer
	const FBoneContainer* BoneContainer;
};

template<class PoseType>
struct FCSPose
{
	// Set up our index type based on the type of pose we are manipulating
	typedef typename PoseType::BoneIndexType BoneIndexType;

	// Init Pose
	void InitPose(const FBoneContainer* InBoneContainer)
	{
		Pose.SetBoneContainer(InBoneContainer);
		Pose.ResetToRefPose();
		ComponentSpaceFlags.Empty(Pose.GetNumBones());
		ComponentSpaceFlags.AddZeroed(Pose.GetNumBones());
		ComponentSpaceFlags[0] = 1;
	}

	// Init Pose
	void InitPose(const PoseType& SrcPose)
	{
		Pose.InitFrom(SrcPose);
		ComponentSpaceFlags.Empty(Pose.GetNumBones());
		ComponentSpaceFlags.AddZeroed(Pose.GetNumBones());
		ComponentSpaceFlags[0] = 1;
	}

	// Copy Pose
	template <typename OtherPoseType>
	void CopyPose(const OtherPoseType& SrcPose)
	{
		Pose.CopyBonesFrom(SrcPose.GetPose());
		ComponentSpaceFlags = SrcPose.GetComponentSpaceFlags();
	}

	const PoseType& GetPose() const { return Pose; }
	const TCustomBoneIndexArray<uint8, BoneIndexType>& GetComponentSpaceFlags() const { return ComponentSpaceFlags; }

	// Get transform for supplied bone in local space
	FTransform GetLocalSpaceTransform(BoneIndexType BoneIndex);

	// Get Transform for supplied bone in component space
	const FTransform& GetComponentSpaceTransform(BoneIndexType BoneIndex);

	// Set the transform for the supplied bone 
	void SetComponentSpaceTransform(BoneIndexType BoneIndex, const FTransform& NewTransform);

	// Calculate the component space transform for the supplied bone
	void CalculateComponentSpaceTransform(BoneIndexType BoneIndex);

	/**
	* Convert Bone to Local Space.
	*/
	void ConvertBoneToLocalSpace(BoneIndexType BoneIndex);

	/**
	* Set a bunch of Component Space Bone Transforms.
	* Do this safely by insuring that Parents are already in Component Space,
	* and any Component Space children are converted back to Local Space before hand.
	*/
	void SafeSetCSBoneTransforms(const TArray<struct FBoneTransform> & BoneTransforms);

	/**
	* Blends Component Space transforms to MeshPose in Local Space.
	* Used by SkelControls to apply their transforms.
	*
	* The tricky bit is that SkelControls deliver their transforms in Component Space,
	* But the blending is done in Local Space. Also we need to refresh any Children they have
	* that has been previously converted to Component Space.
	*/
	void LocalBlendCSBoneTransforms(const TArray<struct FBoneTransform>& BoneTransforms, float Alpha);

	// Convert any component space transforms back to local space
	void ConvertToLocalPoses(PoseType& OutPose) const;

protected:
	PoseType Pose;

	// Flags to track each bones current state (0 means local pose, 1 means component space pose)
	TCustomBoneIndexArray<uint8, BoneIndexType> ComponentSpaceFlags;

	// Cached bone mask array to avoid reallocations
	TCustomBoneIndexArray<uint8, BoneIndexType> BoneMask;

	// Cached conversion array for this pose, to save on allocations each frame
	TArray<FCompactPoseBoneIndex> BonesToConvert;
};

template<class PoseType>
FTransform FCSPose<PoseType>::GetLocalSpaceTransform(BoneIndexType BoneIndex)
{
	checkSlow(Pose.IsValid());
	check(Pose.IsValidIndex(BoneIndex));

	// if evaluated, calculate it
	if (ComponentSpaceFlags[BoneIndex])
	{
		const BoneIndexType ParentIndex = Pose.GetParentBoneIndex(BoneIndex);

		if (ParentIndex != INDEX_NONE)
		{
			const FTransform& ParentTransform = GetComponentSpaceTransform(ParentIndex);
			const FTransform& BoneTransform = Pose[BoneIndex];
			// calculate local space
			return BoneTransform.GetRelativeTransform(ParentTransform);
		}
	}

	return Pose[BoneIndex];
}

template<class PoseType>
const FTransform& FCSPose<PoseType>::GetComponentSpaceTransform(BoneIndexType BoneIndex)
{
	checkSlow(Pose.IsValid());
	check(Pose.IsValidIndex(BoneIndex));	// Invalid index supplied. If this came from an FBoneReference make sure you are
											// handling lod changes properly. (for instance: if on an anim node initialize the reference in CacheBones)

	check(!Pose[BoneIndex].ContainsNaN());
	// if not evaluate, calculate it
	if (ComponentSpaceFlags[BoneIndex] == 0)
	{
		CalculateComponentSpaceTransform(BoneIndex);
	}
	check(!Pose[BoneIndex].ContainsNaN());
	return Pose[BoneIndex];
}

template<class PoseType>
void FCSPose<PoseType>::SetComponentSpaceTransform(BoneIndexType BoneIndex, const FTransform& NewTransform)
{
	checkSlow(Pose.IsValid());
	check(Pose.IsValidIndex(BoneIndex));

	// this one forcefully sets component space transform
	Pose[BoneIndex] = NewTransform;
	ComponentSpaceFlags[BoneIndex] = 1;
}

template<class PoseType>
void FCSPose<PoseType>::CalculateComponentSpaceTransform(BoneIndexType BoneIndex)
{
	checkSlow(Pose.IsValid());
	check(ComponentSpaceFlags[BoneIndex] == 0);

	// root is already verified, so root should not come here
	// check AllocateLocalPoses
	const BoneIndexType ParentIndex = Pose.GetParentBoneIndex(BoneIndex);

	// if Parent already has been calculated, use it
	if (ComponentSpaceFlags[ParentIndex] == 0)
	{
		// if Parent hasn't been calculated, also calculate parents
		CalculateComponentSpaceTransform(ParentIndex);
	}

	// current Bones(Index) should contain LocalPoses.
	FTransform& Bone = Pose[BoneIndex];
	FTransform& ParentBone = Pose[ParentIndex];
	check(!Pose[BoneIndex].ContainsNaN());
	check(!Pose[ParentIndex].ContainsNaN());

	FTransform ComponentTransform = Pose[BoneIndex] * Pose[ParentIndex];
	check(!ComponentTransform.ContainsNaN());
	Pose[BoneIndex] = ComponentTransform;
	Pose[BoneIndex].NormalizeRotation();
	check(!Pose[BoneIndex].ContainsNaN());
	ComponentSpaceFlags[BoneIndex] = 1;
}

template<class PoseType>
void FCSPose<PoseType>::ConvertBoneToLocalSpace(BoneIndexType BoneIndex)
{
	checkSlow(Pose.IsValid());

	// If BoneTransform is in Component Space, then convert it.
	// Never convert Root to Local Space.
	if (!BoneIndex.IsRootBone() && ComponentSpaceFlags[BoneIndex] == 1)
	{
		const BoneIndexType ParentIndex = Pose.GetParentBoneIndex(BoneIndex);

		// Verify that our Parent is also in Component Space. That should always be the case.
		check(ComponentSpaceFlags[ParentIndex] == 1);

		// Convert to local space.
		Pose[BoneIndex].SetToRelativeTransform(Pose[ParentIndex]);
		ComponentSpaceFlags[BoneIndex] = 0;
	}
}

template<class PoseType>
void FCSPose<PoseType>::SafeSetCSBoneTransforms(const TArray<struct FBoneTransform> & BoneTransforms)
{
	checkSlow(Pose.IsValid());

	BonesToConvert.Reset();

	// Minimum bone index, we don't need to look at bones prior to this in the pose
	const int32 MinIndex = BoneTransforms[0].BoneIndex.GetInt();

	// Add BoneTransforms indices if they're in component space
	for(const FBoneTransform& Transform : BoneTransforms)
	{
		if(ComponentSpaceFlags[Transform.BoneIndex] == 1)
		{
			BonesToConvert.Add(Transform.BoneIndex);
		}
	}

	// Store the beginning of the child transforms, below we don't need to convert any bone added
	// from BoneTransforms because they're about to be overwritten
	const int32 FirstChildTransform = BonesToConvert.Num();

	FCompactPoseBoneIndexIterator Iter = FCompactPoseBoneIndexIterator(MinIndex);
	FCompactPoseBoneIndexIterator EndIter = Pose.MakeEndIter();

	// Add child bones if they're in component space
	for(; Iter != EndIter; ++Iter)
	{
		const FCompactPoseBoneIndex BoneIndex = *Iter;
		const FCompactPoseBoneIndex ParentIndex = Pose.GetParentBoneIndex(BoneIndex);

		if(ComponentSpaceFlags[BoneIndex] == 1 && BonesToConvert.Contains(ParentIndex))
		{
			BonesToConvert.AddUnique(BoneIndex);
		}
	}

	// Convert the bones, we walk backwards to process children first, the pose iteration above is sorted
	// so we know we already have the right order. We also stop when we get to the bones contained in
	// BoneTransforms because we're about to overwrite them anyway
	const int32 NumToConvert = BonesToConvert.Num();
	for(int32 Idx = NumToConvert - 1; Idx >= FirstChildTransform; --Idx)
	{
		ConvertBoneToLocalSpace(BonesToConvert[Idx]);
	}

	// Finally copy our Component Space transforms
	for (const FBoneTransform& BoneTransform : BoneTransforms)
	{
		const FCompactPoseBoneIndex BoneIndex = BoneTransform.BoneIndex;

		// Make sure our BoneTransforms were in Component Space in the first place, before we overwrite them
		// Only check their parent to do minimal work needed.
		const FCompactPoseBoneIndex ParentBoneIndex = Pose.GetParentBoneIndex(BoneIndex);
		if (ParentBoneIndex != INDEX_NONE && ComponentSpaceFlags[ParentBoneIndex] == 0)
		{
			CalculateComponentSpaceTransform(ParentBoneIndex);
		}

		// Set new Component Space transform.
		SetComponentSpaceTransform(BoneIndex, BoneTransform.Transform);
	}
}

template<class PoseType>
void FCSPose<PoseType>::LocalBlendCSBoneTransforms(const TArray<struct FBoneTransform>& BoneTransforms, float Alpha)
{
	SCOPE_CYCLE_COUNTER(STAT_LocalBlendCSBoneTransforms);

	// if Alpha is small enough, skip
	if (Alpha < ZERO_ANIMWEIGHT_THRESH)
	{
		return;
	}

#if DO_CHECK
	if (BoneTransforms.Num() > 0)
	{
		FCompactPoseBoneIndex LastIndex(BoneTransforms[0].BoneIndex);
		// Make sure bones are sorted in "Parents before Children" order.
		for (int32 I = 1; I < BoneTransforms.Num(); ++I)
		{
			check(BoneTransforms[I].BoneIndex >= LastIndex);
			LastIndex = BoneTransforms[I].BoneIndex;
		}
	}
#endif

	// If we are not doing any blending, do a faster path.
	// Set transforms directly in Component Space. But still refresh children.
	if (Alpha >= 1.f - ZERO_ANIMWEIGHT_THRESH)
	{
		SafeSetCSBoneTransforms(BoneTransforms);
	}
	// Slower blending path.
	else
	{
		// Bone Mask to keep track of which bones have to be converted to local space.
		// This is basically BoneTransforms bones and their children.
		BoneMask.Reset();
		BoneMask.AddZeroed(Pose.GetNumBones());

		TArray<struct FBoneTransform> LocalBoneTransforms;
		LocalBoneTransforms.SetNumUninitialized(BoneTransforms.Num());

		// First, convert BoneTransforms to local space for blending.
		for (int32 Index = 0; Index < BoneTransforms.Num(); Index++)
		{
			const BoneIndexType BoneIndex = BoneTransforms[Index].BoneIndex;
			const BoneIndexType ParentIndex = Pose.GetParentBoneIndex(BoneIndex);

			if (ParentIndex != INDEX_NONE)
			{
				// if BoneTransforms(modified by controllers) contains ParentIndex, it should use that as ParentTransform, not the one from input
				int32 LocalParentIndex = INDEX_NONE;
				for (int32 LocalIndex = 0; LocalIndex < BoneTransforms.Num(); ++LocalIndex)
				{
					if (ParentIndex == BoneTransforms[LocalIndex].BoneIndex)
					{
						LocalParentIndex = LocalIndex;
						break;
					}
				}

				// saves Parent Transform
				const bool bNoParent = LocalParentIndex == INDEX_NONE;
				const FTransform& ParentTransform = bNoParent ? GetComponentSpaceTransform(ParentIndex) : BoneTransforms[LocalParentIndex].Transform;

				LocalBoneTransforms[Index].Transform = BoneTransforms[Index].Transform.GetRelativeTransform(ParentTransform);
				LocalBoneTransforms[Index].BoneIndex = BoneIndex;

				// Mark those bones in Mesh Pose as being required to be in Local Space.
				BoneMask[BoneIndex] = 1;
			}
			else
			{
				// when root is entered as to modify, we don't need to adjust parent index, just clear it
				LocalBoneTransforms[Index].Transform = BoneTransforms[Index].Transform;
				LocalBoneTransforms[Index].BoneIndex = BoneIndex;

				BoneMask[BoneIndex] = 1;
			}
		}

		// Then, convert MeshPose Bones from BoneTransforms list, and their children, to local space if they are not already.
		for (const BoneIndexType BoneIndex : Pose.ForEachBoneIndex())
		{
			const BoneIndexType ParentIndex = Pose.GetParentBoneIndex(BoneIndex);
			// Propagate our BoneMask to children.
			if (ParentIndex != INDEX_NONE)
			{
				BoneMask[BoneIndex] |= BoneMask[ParentIndex];
			}
		}

		for (const BoneIndexType BoneIndex : Pose.ForEachBoneIndexReverse())
		{
			if (!BoneIndex.IsRootBone())
			{
				// If this bone has to be converted to Local Space...
				if (BoneMask[BoneIndex] != 0)
				{
					// .. And is not currently in Local Space, then convert it.
					ConvertBoneToLocalSpace(BoneIndex);
				}
			}
		}

		// Lastly, do the blending in local space.
		for (int32 Index = 0; Index < LocalBoneTransforms.Num(); Index++)
		{
			const FCompactPoseBoneIndex BoneIndex = LocalBoneTransforms[Index].BoneIndex;
			// Make sure this transform is in local space, because we are writing a local space one to it.
			// If we are not in local space, this could mean trouble for our children.
			check((ComponentSpaceFlags[BoneIndex] == 0) || (BoneIndex == 0));

			// No need to normalize rotation since BlendWith() does it.
			Pose[BoneIndex].BlendWith(LocalBoneTransforms[Index].Transform, Alpha);
		}
	}
}

template<class PoseType>
void FCSPose<PoseType>::ConvertToLocalPoses(PoseType& OutPose) const
{
	checkSlow(Pose.IsValid());
	OutPose = Pose;

	// now we need to convert back to local bases
	// only convert back that has been converted to mesh base
	// if it was local base, and if it hasn't been modified
	// that is still okay even if parent is changed, 
	// that doesn't mean this local has to change
	// go from child to parent since I need parent inverse to go back to local
	// root is same, so no need to do Index == 0
	const BoneIndexType RootBoneIndex(0);
	if (ComponentSpaceFlags[RootBoneIndex])
	{
		OutPose[RootBoneIndex] = Pose[RootBoneIndex];
	}

	const int32 NumBones = Pose.GetNumBones();
	for (int32 Index = NumBones - 1; Index > 0; Index--)
	{
		const BoneIndexType BoneIndex(Index);
		if (ComponentSpaceFlags[BoneIndex])
		{
			const BoneIndexType ParentIndex = Pose.GetParentBoneIndex(BoneIndex);
			OutPose[BoneIndex].SetToRelativeTransform(OutPose[ParentIndex]);
			OutPose[BoneIndex].NormalizeRotation();
		}
	}
}