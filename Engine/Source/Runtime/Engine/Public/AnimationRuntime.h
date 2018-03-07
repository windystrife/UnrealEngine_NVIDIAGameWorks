// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationRuntime.h: Skeletal mesh animation utilities
	Should only contain functions needed for runtime animation, no tools.
=============================================================================*/ 

#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "Components/SkinnedMeshComponent.h"
#include "BonePose.h"
#include "Containers/ArrayView.h"

class UBlendSpaceBase;
class USkeletalMeshComponent;
struct FA2CSPose;
struct FA2Pose;
struct FInputBlendPose;

typedef TArray<FTransform> FTransformArrayA2;

/////////////////////////////////////////////////////////
// Templated Transform Blend Functionality

namespace ETransformBlendMode
{
	enum Type
	{
		Overwrite,
		Accumulate
	};
}

template<int32>
FORCEINLINE void BlendTransform(const FTransform& Source, FTransform& Dest, const float BlendWeight);

template<>
FORCEINLINE void BlendTransform<ETransformBlendMode::Overwrite>(const FTransform& Source, FTransform& Dest, const float BlendWeight)
{
	const ScalarRegister VBlendWeight(BlendWeight);
	Dest = Source * VBlendWeight;
}

template<>
FORCEINLINE void BlendTransform<ETransformBlendMode::Accumulate>(const FTransform& Source, FTransform& Dest, const float BlendWeight)
{
	const ScalarRegister VBlendWeight(BlendWeight);
	Dest.AccumulateWithShortestRotation(Source, VBlendWeight);
}

FORCEINLINE void BlendCurves(const TArrayView<const FBlendedCurve> SourceCurves, const TArrayView<const float> SourceWeights, FBlendedCurve& OutCurve)
{
	if (SourceCurves.Num() > 0)
	{
		OutCurve.Override(SourceCurves[0], SourceWeights[0]);

		for (int32 CurveIndex = 1; CurveIndex < SourceCurves.Num(); ++CurveIndex)
		{
			OutCurve.Accumulate(SourceCurves[CurveIndex], SourceWeights[CurveIndex]);
		}
	}
}

/////////////////////////////////////////////////////////
/** Interface used to provide interpolation indices for per bone blends
  *
  */
class ENGINE_API IInterpolationIndexProvider
{
public:
	virtual int32 GetPerBoneInterpolationIndex(int32 BoneIndex, const FBoneContainer& RequiredBones) const = 0;
};

/** In AnimationRunTime Library, we extract animation data based on Skeleton hierarchy, not ref pose hierarchy. 
	Ref pose will need to be re-mapped later **/
class ENGINE_API FAnimationRuntime
{
public:
	static void NormalizeRotations(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms);
	static void NormalizeRotations(FTransformArrayA2& Atoms);

	static void InitializeTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms);
#if DO_GUARD_SLOW
	static bool ContainsNaN(TArray<FBoneIndexType>& RequiredBoneIndices, FA2Pose& Pose);
#endif

	/**
	* Blends together a set of poses, each with a given weight.
	* This function is lightweight, it does not cull out nearly zero weights or check to make sure weights sum to 1.0, the caller should take care of that if needed.
	*
	* The blend is done by taking a weighted sum of each atom, and re-normalizing the quaternion part at the end, not using SLERP.
	* This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendPosesTogether(
		TArrayView<const FCompactPose> SourcePoses,
		TArrayView<const FBlendedCurve> SourceCurves,
		TArrayView<const float> SourceWeights,
		/*out*/ FCompactPose& ResultPose, 
		/*out*/ FBlendedCurve& ResultCurve);

	/**
	* Blends together a set of poses, each with a given weight.
	* This function is lightweight, it does not cull out nearly zero weights or check to make sure weights sum to 1.0, the caller should take care of that if needed.
	*
	* The blend is done by taking a weighted sum of each atom, and re-normalizing the quaternion part at the end, not using SLERP.
	* This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
	*
	* SourceWeightsIndices is used to index SourceWeights, to prevent caller having to supply an ordered weights array 
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendPosesTogether(
		TArrayView<const FCompactPose> SourcePoses,
		TArrayView<const FBlendedCurve> SourceCurves,
		TArrayView<const float> SourceWeights,
		TArrayView<const int32> SourceWeightsIndices,
		/*out*/ FCompactPose& ResultPose,
		/*out*/ FBlendedCurve& ResultCurve);

	/**
	* Blends together a set of poses, each with a given weight.
	* This function is lightweight, it does not cull out nearly zero weights or check to make sure weights sum to 1.0, the caller should take care of that if needed.
	*
	* The blend is done by taking a weighted sum of each atom, and re-normalizing the quaternion part at the end, not using SLERP.
	* This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendPosesTogetherIndirect(
		TArrayView<const FCompactPose* const> SourcePoses,
		TArrayView<const FBlendedCurve* const> SourceCurves,
		TArrayView<const float> SourceWeights,
		/*out*/ FCompactPose& ResultPose,
		/*out*/ FBlendedCurve& ResultCurve);

	/**
	* Blends together two poses.
	* This function is lightweight
	*
	* The blend is done by taking a weighted sum of each atom, and re-normalizing the quaternion part at the end, not using SLERP.
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendTwoPosesTogether(
		const FCompactPose& SourcePose1,
		const FCompactPose& SourcePose2,
		const FBlendedCurve& SourceCurve1,
		const FBlendedCurve& SourceCurve2,
		const float WeightOfPose1,
		/*out*/ FCompactPose& ResultPose,
		/*out*/ FBlendedCurve& ResultCurve);

	/**
	* Blends together a set of poses, each with a given weight.
	* This function is for BlendSpace per bone blending. BlendSampleDataCache contains the weight information
	*
	* This blends in local space
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendTwoPosesTogetherPerBone(
		const FCompactPose& SourcePose1,
		const FCompactPose& SourcePose2,
		const FBlendedCurve& SourceCurve1,
		const FBlendedCurve& SourceCurve2,
		const TArray<float> WeightsOfSource2,
		/*out*/ FCompactPose& ResultPose,
		/*out*/ FBlendedCurve& ResultCurve);


	/**
	* Blends together a set of poses, each with a given weight.
	* This function is for BlendSpace per bone blending. BlendSampleDataCache contains the weight information
	*
	* This blends in local space
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendPosesTogetherPerBone(
		TArrayView<const FCompactPose> SourcePoses,
		TArrayView<const FBlendedCurve> SourceCurves,
		const IInterpolationIndexProvider* InterpolationIndexProvider,
		TArrayView<const FBlendSampleData> BlendSampleDataCache,
		/*out*/ FCompactPose& ResultPose,
		/*out*/ FBlendedCurve& ResultCurve);

	/**
	* Blends together a set of poses, each with a given weight.
	* This function is for BlendSpace per bone blending. BlendSampleDataCache contains the weight information and is indexed using BlendSampleDataCacheIndices, to prevent caller having to supply an ordered array 
	*
	* This blends in local space
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendPosesTogetherPerBone(
		TArrayView<const FCompactPose> SourcePoses,
		TArrayView<const FBlendedCurve> SourceCurves,
		const IInterpolationIndexProvider* InterpolationIndexProvider,
		TArrayView<const FBlendSampleData> BlendSampleDataCache,
		TArrayView<const int32> BlendSampleDataCacheIndices,
		/*out*/ FCompactPose& ResultPose,
		/*out*/ FBlendedCurve& ResultCurve);

	/**
	* Blends together a set of poses, each with a given weight.
	* This function is for BlendSpace per bone blending. BlendSampleDataCache contains the weight information
	*
	* This blends rotation in mesh space, so performance intensive.
	*
	* @param	ResultPose		Output pose of relative bone transforms.
	*/
	static void BlendPosesTogetherPerBoneInMeshSpace(
		TArrayView<FCompactPose> SourcePoses,
		TArrayView<const FBlendedCurve> SourceCurves,
		const UBlendSpaceBase* BlendSpace,
		TArrayView<const FBlendSampleData> BlendSampleDataCache,
		/*out*/ FCompactPose& ResultPose,
		/*out*/ FBlendedCurve& ResultCurve);

	/**
	* Blend Poses per bone weights : The BasePoses + BlendPoses(SourceIndex) * Blend Weights(BoneIndex)
	* Please note BlendWeights are array, so you can define per bone base
	* This supports multi per bone blending, but only one pose as blend at a time per track
	* PerBoneBlendWeights.Num() == Atoms.Num()
	*
	* I had multiple debates about having PerBoneBlendWeights array(for memory reasons),
	* but it so much simplifies multiple purpose for this function instead of searching bonenames or
	* having multiple bone names with multiple weights, and filtering through which one is correct one
	* I assume all those things should be determined before coming here and this only cares about weights
	**/
	static void BlendPosesPerBoneFilter(
		FCompactPose& BasePose, 
		const TArray<FCompactPose>& BlendPoses, 
		FBlendedCurve& BaseCurve, 
		const TArray<FBlendedCurve>& BlendCurves, 
		FCompactPose& OutPose, 
		FBlendedCurve& OutCurve, 
		TArray<FPerBoneBlendWeight>& BoneBlendWeights, 
		bool bMeshSpaceRotationBlending,
		enum ECurveBlendOption::Type CurveBlendOption);

	static void UpdateDesiredBoneWeight(const TArray<FPerBoneBlendWeight>& SrcBoneBlendWeights, TArray<FPerBoneBlendWeight>& TargetBoneBlendWeights, const TArray<float>& BlendWeights);

	/**
	 *	Create Mast Weight for skeleton joints, not per mesh or per required bones
	 *  You'll have to filter properly with correct mesh joint or required boens
	 *  The depth should not change based on LOD or mesh or skeleton
	 *	They still should contain same depth
	 */
	static void CreateMaskWeights(
			TArray<FPerBoneBlendWeight>& BoneBlendWeights,
			const TArray<FInputBlendPose>& BlendFilters, 
			const USkeleton* Skeleton);

	static void CombineWithAdditiveAnimations(
		int32 NumAdditivePoses,
		const FTransformArrayA2** SourceAdditivePoses,
		const float* SourceAdditiveWeights,
		const FBoneContainer& RequiredBones,
		/*inout*/ FTransformArrayA2& Atoms);

	/** Get Reference Component Space Transform */
	static FTransform GetComponentSpaceRefPose(const FCompactPoseBoneIndex& CompactPoseBoneIndex, const FBoneContainer& BoneContainer);

	/** Fill ref pose **/
	static void FillWithRefPose(TArray<FTransform>& OutAtoms, const FBoneContainer& RequiredBones);

#if WITH_EDITOR
	/** fill with retarget base ref pose but this isn't used during run-time, so it always copies all of them */
	static void FillWithRetargetBaseRefPose(FCompactPose& OutPose, const USkeletalMesh* Mesh);
#endif

	/** Convert LocalTransforms into MeshSpaceTransforms over RequiredBones. */
	static void ConvertPoseToMeshSpace(const TArray<FTransform>& LocalTransforms, TArray<FTransform>& MeshSpaceTransforms, const FBoneContainer& RequiredBones);

	/** Convert TargetPose into an AdditivePose, by doing TargetPose = TargetPose - BasePose */
	static void ConvertPoseToAdditive(FCompactPose& TargetPose, const FCompactPose& BasePose);

	/** convert transform to additive */
	static void ConvertTransformToAdditive(FTransform& TargetTrasnform, const FTransform& BaseTransform);

	/** Convert LocalPose into MeshSpaceRotations. Rotations are NOT normalized. */
	static void ConvertPoseToMeshRotation(FCompactPose& LocalPose);

	/** Convert a MeshSpaceRotation pose to Local Space. Rotations are NOT normalized. */
	static void ConvertMeshRotationPoseToLocalSpace(FCompactPose& Pose);

	/** Accumulate Additive Pose based on AdditiveType*/
	static void AccumulateAdditivePose(FCompactPose& BasePose, const FCompactPose& AdditivePose, FBlendedCurve& BaseCurve, const FBlendedCurve& AdditiveCurve, float Weight, enum EAdditiveAnimationType AdditiveType);

private:
	/** Accumulates weighted AdditivePose to BasePose. Rotations are NOT normalized. */
	static void AccumulateLocalSpaceAdditivePoseInternal(FCompactPose& BasePose, const FCompactPose& AdditivePose, float Weight);

	/** Accumulate a MeshSpaceRotation Additive pose to a local pose. Rotations are NOT normalized */
	static void AccumulateMeshSpaceRotationAdditiveToLocalPoseInternal(FCompactPose& BasePose, const FCompactPose& MeshSpaceRotationAdditive, float Weight);
public:

	/** Accumulates weighted AdditivePose to BasePose. Rotations are NOT normalized. */
	static void AccumulateLocalSpaceAdditivePose(FCompactPose& BasePose, const FCompactPose& AdditivePose, FBlendedCurve& BaseCurve, const FBlendedCurve& AdditiveCurve, float Weight) { AccumulateAdditivePose(BasePose, AdditivePose, BaseCurve, AdditiveCurve, Weight, EAdditiveAnimationType::AAT_LocalSpaceBase); }

	/** Accumulate a MeshSpaceRotation Additive pose to a local pose. Rotations are NOT normalized */
	static void AccumulateMeshSpaceRotationAdditiveToLocalPose(FCompactPose& BasePose, const FCompactPose& MeshSpaceRotationAdditive, FBlendedCurve& BaseCurve, const FBlendedCurve& AdditiveCurve, float Weight) { AccumulateAdditivePose(BasePose, MeshSpaceRotationAdditive, BaseCurve, AdditiveCurve, Weight, EAdditiveAnimationType::AAT_RotationOffsetMeshSpace); }


	/** Lerp for BoneTransforms. Stores results in A. Performs A = Lerp(A, B, Alpha);
	* @param A : In/Out FCompactPose.
	* @param B : B In FCompactPose.
	* @param Alpha : Alpha.
	*/
	static void LerpPoses(FCompactPose& PoseA, const FCompactPose& PoseB, FBlendedCurve& CurveA, const FBlendedCurve& CurveB, float Alpha);

	/** Lerp for BoneTransforms. Stores results in A. Performs A = Lerp(A, B, Alpha);
	* @param A : In/Out FCompactPose.
	* @param B : B In FCompactPose.
	* @param Alpha : Alpha.
	*/
	static void LerpPosesPerBone(FCompactPose& PoseA, const FCompactPose& PoseB, FBlendedCurve& CurveA, const FBlendedCurve& CurveB, float Alpha, const TArray<float>& PerBoneWeights);

	/** Lerp for BoneTransforms. Stores results in A. Performs A = Lerp(A, B, Alpha);
	 * @param A : In/Out transform array.
	 * @param B : B In transform array.
	 * @param Alpha : Alpha.
	 * @param RequiredBonesArray : Array of bone indices.
	 */
	static void LerpBoneTransforms(TArray<FTransform>& A, const TArray<FTransform>& B, float Alpha, const TArray<FBoneIndexType>& RequiredBonesArray);

	/** 
	 * Blend Array of Transforms by weight
	 *
	 * @param OutTransform : result
	 */
	static void BlendTransformsByWeight(FTransform& OutTransform, const TArray<FTransform>& Transforms, const TArray<float>& Weights);

	/** 
	 * Advance CurrentTime to CurrentTime + MoveDelta. 
	 * It will handle wrapping if bAllowLooping is true
	 *
	 * return ETypeAdvanceAnim type
	 */
	static enum ETypeAdvanceAnim AdvanceTime(const bool& bAllowLooping, const float& MoveDelta, float& InOutTime, const float& EndTime);

	static void TickBlendWeight(float DeltaTime, float DesiredWeight, float& Weight, float& BlendTime);
	/** 
	 * Apply Weight to the Transform 
	 * Atoms = Weight * Atoms at the end
	 */
	static void ApplyWeightToTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms, float Weight);

	/** 
	 * Get Key Indices (start/end with alpha from start) with input parameter Time, NumFrames
	 * from % from StartKeyIndex, meaning (CurrentKeyIndex(float)-StartKeyIndex)/(EndKeyIndex-StartKeyIndex)
	 * by this Start-End, it will be between 0-(NumFrames-1), not number of Pos/Rot key tracks 
	 **/
	static void GetKeyIndicesFromTime(int32& OutKeyIndex1, int32& OutKeyIndex2, float& OutAlpha, const float Time, const int32 NumFrames, const float SequenceLength);

	/** 
	 *	Utility for taking an array of bone indices and ensuring that all parents are present 
	 *	(ie. all bones between those in the array and the root are present). 
	 *	Note that this must ensure the invariant that parent occur before children in BoneIndices.
	 */
	static void EnsureParentsPresent(TArray<FBoneIndexType>& BoneIndices, const FReferenceSkeleton& RefSkeleton);

	static void ExcludeBonesWithNoParents(const TArray<int32>& BoneIndices, const FReferenceSkeleton& RefSkeleton, TArray<int32>& FilteredRequiredBones);

	/** 
	 * Convert a ComponentSpace FTransform to specified bone space. 
	 * @param	ComponentTransform	The transform of the component. Only used if Space == BCS_WorldSpace
	 * @param	MeshBases			The pose to use when transforming
	 * @param	InOutCSBoneTM		The component space transform to convert
	 * @param	BoneIndex			The bone index of the transform
	 * @param	Space				The space to convert the input transform into.
	 */
	static void ConvertCSTransformToBoneSpace(const FTransform& ComponentTransform, FCSPose<FCompactPose>& MeshBases, FTransform& InOutCSBoneTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space);

	DEPRECATED(4.16, "Please use the ConvertCSTransformToBoneSpace with a transform as the first argument")
	static void ConvertCSTransformToBoneSpace(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, FTransform& InOutCSBoneTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space);

	/** 
	 * Convert a FTransform in a specified bone space to ComponentSpace.
	 * @param	ComponentTransform	The transform of the component. Only used if Space == BCS_WorldSpace
	 * @param	MeshBases			The pose to use when transforming
	 * @param	InOutBoneSpaceTM	The bone transform to convert
	 * @param	BoneIndex			The bone index of the transform
	 * @param	Space				The space that the transform is in.
	 */
	static void ConvertBoneSpaceTransformToCS(const FTransform& ComponentTransform, FCSPose<FCompactPose>& MeshBases, FTransform& InOutBoneSpaceTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space);

	DEPRECATED(4.16, "Please use the ConvertBoneSpaceTransformToCS with a transform as the first argument")
	static void ConvertBoneSpaceTransformToCS(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, FTransform& InOutBoneSpaceTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space);

	// FA2Pose/FA2CSPose Interfaces for template functions
	static FTransform GetSpaceTransform(FA2Pose& Pose, int32 Index);
	static FTransform GetSpaceTransform(FA2CSPose& Pose, int32 Index);
	static void SetSpaceTransform(FA2Pose& Pose, int32 Index, FTransform& NewTransform);
	static void SetSpaceTransform(FA2CSPose& Pose, int32 Index, FTransform& NewTransform);
	// space bases
	static FTransform GetComponentSpaceTransformRefPose(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex);
	static FTransform GetComponentSpaceTransform(const FReferenceSkeleton& RefSkeleton, const TArray<FTransform> &BoneSpaceTransforms, int32 BoneIndex);
	static void FillUpComponentSpaceTransforms(const FReferenceSkeleton& RefSkeleton, const TArray<FTransform> &BoneSpaceTransforms, TArray<FTransform> &ComponentSpaceTransforms);
#if WITH_EDITOR
	static void FillUpComponentSpaceTransformsRefPose(const USkeleton* Skeleton, TArray<FTransform> &ComponentSpaceTransforms);
	static void FillUpComponentSpaceTransformsRetargetBasePose(const USkeleton* Skeleton, TArray<FTransform> &ComponentSpaceTransforms);
#endif

	/* Weight utility functions */
	static FORCEINLINE bool IsFullWeight(float Weight) { return FAnimWeight::IsFullWeight(Weight); }
	static FORCEINLINE bool HasWeight(float Weight) { return FAnimWeight::IsRelevant(Weight); }
	
	/**
	* Combine CurveKeys (that reference morph targets by name) and ActiveAnims (that reference morphs by reference) into the ActiveMorphTargets array.
	*/
 	static void AppendActiveMorphTargets(const USkeletalMesh* InSkeletalMesh, const TMap<FName, float>& InMorphCurveAnims, TArray<FActiveMorphTarget>& InOutActiveMorphTargets, TArray<float>& InOutMorphTargetWeights);

	/**
	* Retarget a single bone transform, to apply right after extraction.
	*
	* @param	MySkeleton			Skeleton this is retargeting
	* @param	RetargetSource		Retarget Source for the retargeting
	* @param	BoneTransform		BoneTransform to read/write from.
	* @param	SkeletonBoneIndex	Bone Index in USkeleton.
	* @param	BoneIndex			Bone Index in Bone Transform array.
	* @param	RequiredBones		BoneContainer
	*/
	static void RetargetBoneTransform(const USkeleton* MySkeleton, const FName& RetargetSource, FTransform& BoneTransform, const int32 SkeletonBoneIndex, const FCompactPoseBoneIndex& BoneIndex, const FBoneContainer& RequiredBones, const bool bIsBakedAdditive);
private:
	/** 
	* Blend Poses per bone weights : The BasePose + BlendPoses(SourceIndex) * Blend Weights(BoneIndex)
	* Please note BlendWeights are array, so you can define per bone base 
	* This supports multi per bone blending, but only one pose as blend at a time per track
	* PerBoneBlendWeights.Num() == Atoms.Num()
	*
	* @note : This blends rotation in mesh space, but translation in local space
	*
	* I had multiple debates about having PerBoneBlendWeights array(for memory reasons),  
	* but it so much simplifies multiple purpose for this function instead of searching bonenames or 
	* having multiple bone names with multiple weights, and filtering through which one is correct one
	* I assume all those things should be determined before coming here and this only cares about weights
	**/
	static void BlendMeshPosesPerBoneWeights(
				FCompactPose& BasePose,
				const TArray<FCompactPose>& BlendPoses,
				FBlendedCurve& BaseCurve, 
				const TArray<FBlendedCurve>& BlendedCurves, 
				const TArray<FPerBoneBlendWeight>& BoneBlendWeights,
				enum ECurveBlendOption::Type CurveBlendOption,
				/*out*/ FCompactPose& OutPose, 
				/*out*/ FBlendedCurve& OutCurve);

	/** 
	* Blend Poses per bone weights : The BasePoses + BlendPoses(SourceIndex) * Blend Weights(BoneIndex)
	* Please note BlendWeights are array, so you can define per bone base 
	* This supports multi per bone blending, but only one pose as blend at a time per track
	* PerBoneBlendWeights.Num() == Atoms.Num()
	*
	* @note : This blends all in local space
	* 
	* I had multiple debates about having PerBoneBlendWeights array(for memory reasons),  
	* but it so much simplifies multiple purpose for this function instead of searching bonenames or 
	* having multiple bone names with multiple weights, and filtering through which one is correct one
	* I assume all those things should be determined before coming here and this only cares about weights
	**/
	static void BlendLocalPosesPerBoneWeights(
			FCompactPose& BasePose,
			const TArray<FCompactPose>& BlendPoses,
			FBlendedCurve& BaseCurve, 
			const TArray<FBlendedCurve>& BlendedCurves, 
			const TArray<FPerBoneBlendWeight>& BoneBlendWeights,
			enum ECurveBlendOption::Type CurveBlendOption,
			/*out*/ FCompactPose& OutPose, 
			/*out*/ FBlendedCurve& OutCurve);
public:
	/** 
	 * Calculate distance how close two strings are. 
	 * By close, it calculates how many operations to transform First to Second 
	 * The return value is [0-MaxLengthString(First, Second)]
	 * 0 means it's identical, Max means it's completely different
	 */
	static int32 GetStringDistance(const FString& First, const FString& Second);
};

