// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationUE4.cpp: Animation runtime utilities
=============================================================================*/ 

#include "AnimationRuntime.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AnimInstance.h"
#include "SkeletalRender.h"

DEFINE_LOG_CATEGORY(LogAnimation);
DEFINE_LOG_CATEGORY(LogRootMotion);

DECLARE_CYCLE_STAT(TEXT("ConvertPoseToMeshRot"), STAT_ConvertPoseToMeshRot, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("ConvertMeshRotPoseToLocalSpace"), STAT_ConvertMeshRotPoseToLocalSpace, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("AccumulateMeshSpaceRotAdditiveToLocalPose"), STAT_AccumulateMeshSpaceRotAdditiveToLocalPose, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("BlendPosesPerBoneFilter"), STAT_BlendPosesPerBoneFilter, STATGROUP_Anim);


//////////////////////////////////////////////////////////////////////////

void FAnimationRuntime::NormalizeRotations(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms)
{
	check( Atoms.Num() == RequiredBones.GetNumBones() );
	const TArray<FBoneIndexType>& RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		Atoms[BoneIndex].NormalizeRotation();
	}
}

void FAnimationRuntime::NormalizeRotations(FTransformArrayA2& Atoms)
{
	for (int32 BoneIndex = 0; BoneIndex < Atoms.Num(); BoneIndex++)
	{
		Atoms[BoneIndex].NormalizeRotation();
	}
}

void FAnimationRuntime::InitializeTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms)
{
	check( Atoms.Num() == RequiredBones.GetNumBones() );
	const TArray<FBoneIndexType>& RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		Atoms[BoneIndex].SetIdentity();
	}
}

template <int32 TRANSFORM_BLEND_MODE>
FORCEINLINE void BlendPose(const FTransformArrayA2& SourcePoses, FTransformArrayA2& ResultAtoms, const TArray<FBoneIndexType>& RequiredBoneIndices, const float BlendWeight)
{
	for (int32 i = 0; i < RequiredBoneIndices.Num(); ++i)
	{
		const int32 BoneIndex = RequiredBoneIndices[i];
		BlendTransform<TRANSFORM_BLEND_MODE>(SourcePoses[BoneIndex], ResultAtoms[BoneIndex], BlendWeight);
	}
}

template <int32 TRANSFORM_BLEND_MODE>
FORCEINLINE void BlendPose(const FCompactPose& SourcePose, FCompactPose& ResultPose, const float BlendWeight)
{
	for (FCompactPoseBoneIndex BoneIndex : SourcePose.ForEachBoneIndex())
	{
		BlendTransform<TRANSFORM_BLEND_MODE>(SourcePose[BoneIndex], ResultPose[BoneIndex], BlendWeight);
	}
}

FORCEINLINE void BlendCurves(const TArrayView<const FBlendedCurve> SourceCurves, const TArrayView<const float> SourceWeights, const TArrayView<const int32> SourceWeightsIndices, FBlendedCurve& OutCurve)
{
	if (SourceCurves.Num() > 0)
	{
		OutCurve.Override(SourceCurves[0], SourceWeights[SourceWeightsIndices[0]]);

		for (int32 CurveIndex = 1; CurveIndex<SourceCurves.Num(); ++CurveIndex)
		{
			OutCurve.Accumulate(SourceCurves[CurveIndex], SourceWeights[SourceWeightsIndices[CurveIndex]]);
		}
	}
}

FORCEINLINE void BlendCurves(const TArrayView<const FBlendedCurve* const> SourceCurves, const TArrayView<const float> SourceWeights, FBlendedCurve& OutCurve)
{
	if(SourceCurves.Num() > 0)
	{
		OutCurve.Override(*SourceCurves[0], SourceWeights[0]);

		for(int32 CurveIndex=1; CurveIndex<SourceCurves.Num(); ++CurveIndex)
		{
			OutCurve.Accumulate(*SourceCurves[CurveIndex], SourceWeights[CurveIndex]);
		}
	}
}

FORCEINLINE void BlendCurves(const TArrayView<const FBlendedCurve* const> SourceCurves, const TArrayView<const float> SourceWeights, FBlendedCurve& OutCurve, ECurveBlendOption::Type BlendOption)
{
	if(SourceCurves.Num() > 0)
	{
		if (BlendOption == ECurveBlendOption::Type::BlendByWeight)
		{
			BlendCurves(SourceCurves, SourceWeights, OutCurve);
		}
		else if (BlendOption == ECurveBlendOption::Type::NormalizeByWeight)
		{
			float SumOfWeight = 0.f;
			for (const auto& Weight : SourceWeights)
			{
				SumOfWeight += Weight;
			}

			if (FAnimWeight::IsRelevant(SumOfWeight))
			{
				TArray<float> NormalizeSourceWeights;
				NormalizeSourceWeights.AddUninitialized(SourceWeights.Num());
				for(int32 Idx=0; Idx<SourceWeights.Num(); ++Idx)
				{
					NormalizeSourceWeights[Idx] = SourceWeights[Idx] / SumOfWeight;
				}

				BlendCurves(SourceCurves, NormalizeSourceWeights, OutCurve);
			}
			else
			{
				BlendCurves(SourceCurves, SourceWeights, OutCurve);
			}
		}
		else
		{
			OutCurve.Override(*SourceCurves[0], SourceWeights[0]);

			for(int32 CurveIndex=1; CurveIndex<SourceCurves.Num(); ++CurveIndex)
			{
				OutCurve.Combine(*SourceCurves[CurveIndex]);
			}
		}
	}
}

void FAnimationRuntime::BlendPosesTogether(
	const TArrayView<const FCompactPose> SourcePoses,
	const TArrayView<const FBlendedCurve> SourceCurves,
	const TArrayView<const float> SourceWeights,
	/*out*/ FCompactPose& ResultPose, 
	/*out*/ FBlendedCurve& ResultCurve)
{
	check(SourcePoses.Num() > 0);

	BlendPose<ETransformBlendMode::Overwrite>(SourcePoses[0], ResultPose, SourceWeights[0]);

	for (int32 PoseIndex = 1; PoseIndex < SourcePoses.Num(); ++PoseIndex)
	{
		BlendPose<ETransformBlendMode::Accumulate>(SourcePoses[PoseIndex], ResultPose, SourceWeights[PoseIndex]);
	}

	// Ensure that all of the resulting rotations are normalized
	if (SourcePoses.Num() > 1)
	{
		ResultPose.NormalizeRotations();
	}

	// curve blending if exists
	if (SourceCurves.Num() > 0)
	{
		BlendCurves(SourceCurves, SourceWeights, ResultCurve);
	}
}

void FAnimationRuntime::BlendPosesTogether(
	const TArrayView<const FCompactPose> SourcePoses,
	const TArrayView<const FBlendedCurve> SourceCurves,
	const TArrayView<const float> SourceWeights,
	const TArrayView<const int32> SourceWeightsIndices,
	/*out*/ FCompactPose& ResultPose,
	/*out*/ FBlendedCurve& ResultCurve)
{
	check(SourcePoses.Num() > 0);

	BlendPose<ETransformBlendMode::Overwrite>(SourcePoses[0], ResultPose, SourceWeights[SourceWeightsIndices[0]]);

	for (int32 PoseIndex = 1; PoseIndex < SourcePoses.Num(); ++PoseIndex)
	{
		BlendPose<ETransformBlendMode::Accumulate>(SourcePoses[PoseIndex], ResultPose, SourceWeights[SourceWeightsIndices[PoseIndex]]);
	}

	// Ensure that all of the resulting rotations are normalized
	if (SourcePoses.Num() > 1)
	{
		ResultPose.NormalizeRotations();
	}

	// curve blending if exists
	if (SourceCurves.Num() > 0)
	{
		BlendCurves(SourceCurves, SourceWeights, SourceWeightsIndices, ResultCurve);
	}
}

void FAnimationRuntime::BlendPosesTogetherIndirect(
	const TArrayView<const FCompactPose* const> SourcePoses,
	const TArrayView<const FBlendedCurve* const> SourceCurves,
	const TArrayView<const float> SourceWeights,
	/*out*/ FCompactPose& ResultPose,
	/*out*/ FBlendedCurve& ResultCurve)
{
	check(SourcePoses.Num() > 0);

	BlendPose<ETransformBlendMode::Overwrite>(*SourcePoses[0], ResultPose, SourceWeights[0]);

	for (int32 PoseIndex = 1; PoseIndex < SourcePoses.Num(); ++PoseIndex)
	{
		BlendPose<ETransformBlendMode::Accumulate>(*SourcePoses[PoseIndex], ResultPose, SourceWeights[PoseIndex]);
	}

	// Ensure that all of the resulting rotations are normalized
	if (SourcePoses.Num() > 1)
	{
		ResultPose.NormalizeRotations();
	}

	if (SourceCurves.Num() > 0)
	{
		BlendCurves(SourceCurves, SourceWeights, ResultCurve);
	}
}

void FAnimationRuntime::BlendTwoPosesTogether(
	const FCompactPose& SourcePose1,
	const FCompactPose& SourcePose2,
	const FBlendedCurve& SourceCurve1,
	const FBlendedCurve& SourceCurve2,
	const float			WeightOfPose1,
	/*out*/ FCompactPose& ResultPose,
	/*out*/ FBlendedCurve& ResultCurve)
{
	BlendPose<ETransformBlendMode::Overwrite>(SourcePose1, ResultPose, WeightOfPose1);
	BlendPose<ETransformBlendMode::Accumulate>(SourcePose2, ResultPose, 1.f - WeightOfPose1);

	// Ensure that all of the resulting rotations are normalized
	ResultPose.NormalizeRotations();
	ResultCurve.Lerp(SourceCurve1, SourceCurve2, 1.f - WeightOfPose1);
}

void FAnimationRuntime::BlendTwoPosesTogetherPerBone(
	const FCompactPose& SourcePose1,
	const FCompactPose& SourcePose2,
	const FBlendedCurve& SourceCurve1,
	const FBlendedCurve& SourceCurve2,
	const TArray<float> WeightsOfSource2,
	/*out*/ FCompactPose& ResultPose,
	/*out*/ FBlendedCurve& ResultCurve)
{
	
	for (FCompactPoseBoneIndex BoneIndex : ResultPose.ForEachBoneIndex())
	{
		const float BlendWeight = WeightsOfSource2[BoneIndex.GetInt()];
		if (FAnimationRuntime::IsFullWeight(BlendWeight))
		{
			ResultPose[BoneIndex] = SourcePose2[BoneIndex];
		}
		// if it doens't have weight, take source pose 1
		else if (FAnimationRuntime::HasWeight(BlendWeight))
		{
			BlendTransform<ETransformBlendMode::Overwrite>(SourcePose1[BoneIndex], ResultPose[BoneIndex], 1.f - BlendWeight);
			BlendTransform<ETransformBlendMode::Accumulate>(SourcePose2[BoneIndex], ResultPose[BoneIndex], BlendWeight);
		}
		else
		{
			ResultPose[BoneIndex] = SourcePose1[BoneIndex];
		}
	}

	// Ensure that all of the resulting rotations are normalized
	ResultPose.NormalizeRotations();

	// @note : This isn't perfect as curve can link to joint, and it would be the best to use that information
	// but that is very expensive option as we have to have another indirect look up table to search. 
	// For now, replacing with combine (non-zero will be overriden)
	// in the future, we might want to do this outside if we want per bone blend to apply curve also UE-39182
	ResultCurve.Override(SourceCurve1);
	ResultCurve.Combine(SourceCurve2);
}

template <int32 TRANSFORM_BLEND_MODE>
void BlendPosePerBone(const TArray<FBoneIndexType>& RequiredBoneIndices, const TArray<int32>& PerBoneIndices, const FBlendSampleData& BlendSampleDataCache, FTransformArrayA2& ResultAtoms, const FTransformArrayA2& SourceAtoms)
{
	const float BlendWeight = BlendSampleDataCache.GetWeight();
	for (int32 i = 0; i < RequiredBoneIndices.Num(); ++i)
	{
		const int32 BoneIndex = RequiredBoneIndices[i];
		const int32 PerBoneIndex = PerBoneIndices[i];
		if (PerBoneIndex == INDEX_NONE || !BlendSampleDataCache.PerBoneBlendData.IsValidIndex(PerBoneIndex))
		{
			BlendTransform<TRANSFORM_BLEND_MODE>(SourceAtoms[BoneIndex], ResultAtoms[BoneIndex], BlendWeight);
		}
		else
		{
			BlendTransform<TRANSFORM_BLEND_MODE>(SourceAtoms[BoneIndex], ResultAtoms[BoneIndex], BlendSampleDataCache.PerBoneBlendData[PerBoneIndex]);
		}
	}
}

template <int32 TRANSFORM_BLEND_MODE>
void BlendPosePerBone(const TArray<int32>& PerBoneIndices, const FBlendSampleData& BlendSampleDataCache, FCompactPose& ResultPose, const FCompactPose& SourcePose)
{
	const float BlendWeight = BlendSampleDataCache.GetWeight();
	for (FCompactPoseBoneIndex BoneIndex : SourcePose.ForEachBoneIndex())
	{
		const int32 PerBoneIndex = PerBoneIndices[BoneIndex.GetInt()];
		if (PerBoneIndex == INDEX_NONE || !BlendSampleDataCache.PerBoneBlendData.IsValidIndex(PerBoneIndex))
		{
			BlendTransform<TRANSFORM_BLEND_MODE>(SourcePose[BoneIndex], ResultPose[BoneIndex], BlendWeight);
		}
		else
		{
			BlendTransform<TRANSFORM_BLEND_MODE>(SourcePose[BoneIndex], ResultPose[BoneIndex], BlendSampleDataCache.PerBoneBlendData[PerBoneIndex]);
		}
	}
}

void FAnimationRuntime::BlendPosesTogetherPerBone(
	const TArrayView<const FCompactPose> SourcePoses,
	const TArrayView<const FBlendedCurve> SourceCurves,
	const IInterpolationIndexProvider* InterpolationIndexProvider,
	const TArrayView<const FBlendSampleData> BlendSampleDataCache,
	/*out*/ FCompactPose& ResultPose,
	/*out*/ FBlendedCurve& ResultCurve)
{
	check(SourcePoses.Num() > 0);

	const TArray<FBoneIndexType>& RequiredBoneIndices = ResultPose.GetBoneContainer().GetBoneIndicesArray();

	TArray<int32> PerBoneIndices;
	PerBoneIndices.AddUninitialized(ResultPose.GetNumBones());
	for (int32 BoneIndex = 0; BoneIndex < PerBoneIndices.Num(); ++BoneIndex)
	{
		PerBoneIndices[BoneIndex] = InterpolationIndexProvider->GetPerBoneInterpolationIndex(RequiredBoneIndices[BoneIndex], ResultPose.GetBoneContainer());
	}

	BlendPosePerBone<ETransformBlendMode::Overwrite>(PerBoneIndices, BlendSampleDataCache[0], ResultPose, SourcePoses[0]);

	for (int32 i = 1; i < SourcePoses.Num(); ++i)
	{
		BlendPosePerBone<ETransformBlendMode::Accumulate>(PerBoneIndices, BlendSampleDataCache[i], ResultPose, SourcePoses[i]);
	}

	// Ensure that all of the resulting rotations are normalized
	ResultPose.NormalizeRotations();

	if (SourceCurves.Num() > 0)
	{
		TArray<float, TInlineAllocator<16>> SourceWeights;
		SourceWeights.AddUninitialized(BlendSampleDataCache.Num());
		for (int32 CacheIndex=0; CacheIndex<BlendSampleDataCache.Num(); ++CacheIndex)
		{
			SourceWeights[CacheIndex] = BlendSampleDataCache[CacheIndex].TotalWeight;
		}

		BlendCurves(SourceCurves, SourceWeights, ResultCurve);
	}
}

void FAnimationRuntime::BlendPosesTogetherPerBone(
	const TArrayView<const FCompactPose> SourcePoses,
	const TArrayView<const FBlendedCurve> SourceCurves,
	const IInterpolationIndexProvider* InterpolationIndexProvider,
	const TArrayView<const FBlendSampleData> BlendSampleDataCache,
	const TArrayView<const int32> BlendSampleDataCacheIndices,
	/*out*/ FCompactPose& ResultPose,
	/*out*/ FBlendedCurve& ResultCurve)
{
	check(SourcePoses.Num() > 0);

	const TArray<FBoneIndexType>& RequiredBoneIndices = ResultPose.GetBoneContainer().GetBoneIndicesArray();

	TArray<int32> PerBoneIndices;
	PerBoneIndices.AddUninitialized(ResultPose.GetNumBones());
	for (int32 BoneIndex = 0; BoneIndex < PerBoneIndices.Num(); ++BoneIndex)
	{
		PerBoneIndices[BoneIndex] = InterpolationIndexProvider->GetPerBoneInterpolationIndex(RequiredBoneIndices[BoneIndex], ResultPose.GetBoneContainer());
	}

	BlendPosePerBone<ETransformBlendMode::Overwrite>(PerBoneIndices, BlendSampleDataCache[BlendSampleDataCacheIndices[0]], ResultPose, SourcePoses[0]);

	for (int32 i = 1; i < SourcePoses.Num(); ++i)
	{
		BlendPosePerBone<ETransformBlendMode::Accumulate>(PerBoneIndices, BlendSampleDataCache[BlendSampleDataCacheIndices[i]], ResultPose, SourcePoses[i]);
	}

	// Ensure that all of the resulting rotations are normalized
	ResultPose.NormalizeRotations();

	if (SourceCurves.Num() > 0)
	{
		TArray<float, TInlineAllocator<16>> SourceWeights;
		SourceWeights.AddUninitialized(BlendSampleDataCacheIndices.Num());
		for (int32 CacheIndex = 0; CacheIndex < BlendSampleDataCacheIndices.Num(); ++CacheIndex)
		{
			SourceWeights[CacheIndex] = BlendSampleDataCache[BlendSampleDataCacheIndices[CacheIndex]].TotalWeight;
		}

		BlendCurves(SourceCurves, SourceWeights, ResultCurve);
	}
}

void FAnimationRuntime::BlendPosesTogetherPerBoneInMeshSpace(
	const TArrayView<FCompactPose> SourcePoses,
	const TArrayView<const FBlendedCurve> SourceCurves,
	const UBlendSpaceBase* BlendSpace,
	const TArrayView<const FBlendSampleData> BlendSampleDataCache,
	/*out*/ FCompactPose& ResultPose,
	/*out*/ FBlendedCurve& ResultCurve)
{
	FQuat NewRotation;
	USkeleton* Skeleton = BlendSpace->GetSkeleton();

	// all this is going to do is to convert SourcePoses.Rotation to be mesh space, and then once it goes through BlendPosesTogetherPerBone, convert back to local
	for (FCompactPose& Pose : SourcePoses)
	{
		for (const FCompactPoseBoneIndex BoneIndex : Pose.ForEachBoneIndex())
		{
			const FCompactPoseBoneIndex ParentIndex = Pose.GetParentBoneIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				NewRotation = Pose[ParentIndex].GetRotation()*Pose[BoneIndex].GetRotation();
				NewRotation.Normalize();
			}
			else
			{
				NewRotation = Pose[BoneIndex].GetRotation();
			}

			// now copy back to SourcePoses
			Pose[BoneIndex].SetRotation(NewRotation);
		}
	}

	// now we have mesh space rotation, call BlendPosesTogetherPerBone
	BlendPosesTogetherPerBone(SourcePoses, SourceCurves, BlendSpace, BlendSampleDataCache, ResultPose, ResultCurve);

	// now result atoms has the output with mesh space rotation. Convert back to local space, start from back
	for (const FCompactPoseBoneIndex BoneIndex : ResultPose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex ParentIndex = ResultPose.GetParentBoneIndex(BoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			const FQuat LocalBlendQuat = ResultPose[ParentIndex].GetRotation().Inverse()*ResultPose[BoneIndex].GetRotation();
			ResultPose[BoneIndex].SetRotation(LocalBlendQuat);
			ResultPose[BoneIndex].NormalizeRotation();
		}
	}
}

void FAnimationRuntime::LerpPoses(FCompactPose& PoseA, const FCompactPose& PoseB, FBlendedCurve& CurveA, const FBlendedCurve& CurveB, float Alpha)
{
	// If pose A is full weight, we're set.
	if (FAnimWeight::IsRelevant(Alpha))
	{
		// Make sure poses are compatible with each other.
		check(&PoseA.GetBoneContainer() == &PoseB.GetBoneContainer());

		// If pose 2 is full weight, just copy, no need to blend.
		if (FAnimWeight::IsFullWeight(Alpha))
		{
			PoseA.CopyBonesFrom(PoseB);
			CurveA.CopyFrom(CurveB);
		}
		else
		{
			const ScalarRegister VWeightOfPose1(1.f - Alpha);
			const ScalarRegister VWeightOfPose2(Alpha);
			for (FCompactPoseBoneIndex BoneIndex : PoseA.ForEachBoneIndex())
			{
				FTransform& InOutBoneTransform1 = PoseA[BoneIndex];
				InOutBoneTransform1 *= VWeightOfPose1;

				const FTransform& BoneTransform2 = PoseB[BoneIndex];
				InOutBoneTransform1.AccumulateWithShortestRotation(BoneTransform2, VWeightOfPose2);

				InOutBoneTransform1.NormalizeRotation();
			}

			CurveA.LerpTo(CurveB, Alpha);
		}
	}
}

void FAnimationRuntime::LerpPosesPerBone(FCompactPose& PoseA, const FCompactPose& PoseB, FBlendedCurve& CurveA, const FBlendedCurve& CurveB, float Alpha, const TArray<float>& PerBoneWeights)
{
	// If pose A is full weight, we're set.
	if (FAnimWeight::IsRelevant(Alpha))
	{
		// Make sure poses are compatible with each other.
		check(&PoseA.GetBoneContainer() == &PoseB.GetBoneContainer());

		for (FCompactPoseBoneIndex BoneIndex : PoseA.ForEachBoneIndex())
		{
			const float BoneAlpha = Alpha * PerBoneWeights[BoneIndex.GetInt()];
			if (FAnimWeight::IsRelevant(BoneAlpha))
			{
				const ScalarRegister VWeightOfPose1(1.f - BoneAlpha);
				const ScalarRegister VWeightOfPose2(BoneAlpha);

				FTransform& InOutBoneTransform1 = PoseA[BoneIndex];
				InOutBoneTransform1 *= VWeightOfPose1;

				const FTransform& BoneTransform2 = PoseB[BoneIndex];
				InOutBoneTransform1.AccumulateWithShortestRotation(BoneTransform2, VWeightOfPose2);

				InOutBoneTransform1.NormalizeRotation();
			}
		}

		// @note : This isn't perfect as curve can link to joint, and it would be the best to use that information
		// but that is very expensive option as we have to have another indirect look up table to search. 
		// For now, replacing with combine (non-zero will be overridden)
		// in the future, we might want to do this outside if we want per bone blend to apply curve also UE-39182
		CurveA.Combine(CurveB);
	}
}

void FAnimationRuntime::LerpBoneTransforms(TArray<FTransform>& A, const TArray<FTransform>& B, float Alpha, const TArray<FBoneIndexType>& RequiredBonesArray)
{
	if (FAnimWeight::IsFullWeight(Alpha))
	{
		A = B;
	}
	else if (FAnimWeight::IsRelevant(Alpha))
	{
		FTransform* ATransformData = A.GetData(); 
		const FTransform* BTransformData = B.GetData();
		const ScalarRegister VAlpha(Alpha);
		const ScalarRegister VOneMinusAlpha(1.f - Alpha);

		for (int32 Index=0; Index<RequiredBonesArray.Num(); Index++)
		{
			const int32& BoneIndex = RequiredBonesArray[Index];
			FTransform* TA = ATransformData + BoneIndex;
			const FTransform* TB = BTransformData + BoneIndex;

			*TA *= VOneMinusAlpha;
			TA->AccumulateWithShortestRotation(*TB, VAlpha);
			TA->NormalizeRotation();

// 			TA->BlendWith(*TB, Alpha);
		}
	}
}

void FAnimationRuntime::BlendTransformsByWeight(FTransform& OutTransform, const TArray<FTransform>& Transforms, const TArray<float>& Weights)
{
	int32 NumBlends = Transforms.Num();
	check(Transforms.Num() == Weights.Num());

	if (NumBlends == 0)
	{
		OutTransform = FTransform::Identity;
	}
	else if (NumBlends == 1)
	{
		OutTransform = Transforms[0];
	}
	else
	{
		// @todo : change this to be veoctorized or move to Ftransform
		FVector		OutTranslation = Transforms[0].GetTranslation() * Weights[0];
		FQuat		OutRotation = Transforms[0].GetRotation() * Weights[0];
		FVector		OutScale = Transforms[0].GetScale3D() * Weights[0];

		// otherwise we just purely blend by number, and then later we normalize
		for (int32 Index = 1; Index < NumBlends; ++Index)
		{
			// Simple linear interpolation for translation and scale.
			OutTranslation = FMath::Lerp(OutTranslation, Transforms[Index].GetTranslation(), Weights[Index]);
			OutScale = FMath::Lerp(OutScale, Transforms[Index].GetScale3D(), Weights[Index]);
			OutRotation = FQuat::FastLerp(OutRotation, Transforms[Index].GetRotation(), Weights[Index]);
		}

		OutRotation.Normalize();
		OutTransform = FTransform(OutRotation, OutTranslation, OutScale);
	}
}

void FAnimationRuntime::CombineWithAdditiveAnimations(int32 NumAdditivePoses, const FTransformArrayA2** SourceAdditivePoses, const float* SourceAdditiveWeights, const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms)
{
	const TArray<FBoneIndexType>& RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 PoseIndex = 0; PoseIndex < NumAdditivePoses; ++PoseIndex)
	{
		const ScalarRegister VBlendWeight(SourceAdditiveWeights[PoseIndex]);
		const FTransformArrayA2& SourceAtoms = *SourceAdditivePoses[PoseIndex];

		for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
		{
			const int32 BoneIndex = RequiredBoneIndices[j];
			FTransform SourceAtom = SourceAtoms[BoneIndex];
			FTransform::BlendFromIdentityAndAccumulate(Atoms[BoneIndex], SourceAtom, VBlendWeight);
		}
	}
}

void FAnimationRuntime::ConvertTransformToAdditive(FTransform& TargetTransform, const FTransform& BaseTransform)
{
	TargetTransform.SetRotation(TargetTransform.GetRotation() * BaseTransform.GetRotation().Inverse());
	TargetTransform.SetTranslation(TargetTransform.GetTranslation() - BaseTransform.GetTranslation());
	// additive scale considers how much it grow or lower
	// in order to support blending between different additive scale, we save [(target scale)/(source scale) - 1.f], and this can blend with 
	// other delta scale value
	// when we apply to the another scale, we apply scale * (1 + [additive scale])
	TargetTransform.SetScale3D(TargetTransform.GetScale3D() * BaseTransform.GetSafeScaleReciprocal(BaseTransform.GetScale3D()) - 1.f);
	TargetTransform.NormalizeRotation();
}

void FAnimationRuntime::ConvertPoseToAdditive(FCompactPose& TargetPose, const FCompactPose& BasePose)
{
	for (FCompactPoseBoneIndex BoneIndex : BasePose.ForEachBoneIndex())
	{
		FTransform& TargetTransform = TargetPose[BoneIndex];
		const FTransform& BaseTransform = BasePose[BoneIndex];

		ConvertTransformToAdditive(TargetTransform, BaseTransform);
	}
}

void FAnimationRuntime::ConvertPoseToMeshRotation(FCompactPose& LocalPose)
{
	SCOPE_CYCLE_COUNTER(STAT_ConvertPoseToMeshRot);

	// Convert all rotations to mesh space
	// only the root bone doesn't have a parent. So skip it to save a branch in the iteration.
	for (FCompactPoseBoneIndex BoneIndex(1); BoneIndex < LocalPose.GetNumBones(); ++BoneIndex)
	{
		const FCompactPoseBoneIndex ParentIndex = LocalPose.GetParentBoneIndex(BoneIndex);

		const FQuat MeshSpaceRotation = LocalPose[ParentIndex].GetRotation() * LocalPose[BoneIndex].GetRotation();
		LocalPose[BoneIndex].SetRotation(MeshSpaceRotation);
	}
}

void FAnimationRuntime::ConvertMeshRotationPoseToLocalSpace(FCompactPose& Pose)
{
	SCOPE_CYCLE_COUNTER(STAT_ConvertMeshRotPoseToLocalSpace);

	// Convert all rotations to mesh space
	// only the root bone doesn't have a parent. So skip it to save a branch in the iteration.
	for (FCompactPoseBoneIndex BoneIndex(Pose.GetNumBones()-1); BoneIndex > 0; --BoneIndex)
	{
		const FCompactPoseBoneIndex ParentIndex = Pose.GetParentBoneIndex(BoneIndex);

		FQuat LocalSpaceRotation = Pose[ParentIndex].GetRotation().Inverse() * Pose[BoneIndex].GetRotation();
		Pose[BoneIndex].SetRotation(LocalSpaceRotation);
	}
}

void FAnimationRuntime::AccumulateAdditivePose(FCompactPose& BasePose, const FCompactPose& AdditivePose, FBlendedCurve& BaseCurve, const FBlendedCurve& AdditiveCurve, float Weight, enum EAdditiveAnimationType AdditiveType)
{
	if (AdditiveType == AAT_RotationOffsetMeshSpace)
	{
		AccumulateMeshSpaceRotationAdditiveToLocalPoseInternal(BasePose, AdditivePose, Weight);
	}
	else
	{
		AccumulateLocalSpaceAdditivePoseInternal(BasePose, AdditivePose, Weight);
	}

	// if curve exists, accumulate with the weight, 
	BaseCurve.Accumulate(AdditiveCurve, Weight);
	// normalize
	BasePose.NormalizeRotations();
}

void FAnimationRuntime::AccumulateLocalSpaceAdditivePoseInternal(FCompactPose& BasePose, const FCompactPose& AdditivePose, float Weight)
{
	if (FAnimWeight::IsRelevant(Weight))
	{
		const ScalarRegister VBlendWeight(Weight);
		if (FAnimWeight::IsFullWeight(Weight))
		{
			// fast path, no need to weight additive.
			for (FCompactPoseBoneIndex BoneIndex : BasePose.ForEachBoneIndex())
			{
				BasePose[BoneIndex].AccumulateWithAdditiveScale(AdditivePose[BoneIndex], VBlendWeight);
			}
		}
		else
		{
			// Slower path w/ weighting
			for (FCompactPoseBoneIndex BoneIndex : BasePose.ForEachBoneIndex())
			{
				// copy additive, because BlendFromIdentityAndAccumulate modifies it.
				FTransform Additive = AdditivePose[BoneIndex];
				FTransform::BlendFromIdentityAndAccumulate(BasePose[BoneIndex], Additive, VBlendWeight);
			}
		}
	}
}

void FAnimationRuntime::AccumulateMeshSpaceRotationAdditiveToLocalPoseInternal(FCompactPose& BasePose, const FCompactPose& MeshSpaceRotationAdditive, float Weight)
{
	SCOPE_CYCLE_COUNTER(STAT_AccumulateMeshSpaceRotAdditiveToLocalPose);

	if (FAnimWeight::IsRelevant(Weight))
	{
		// Convert base pose from local space to mesh space rotation.
		FAnimationRuntime::ConvertPoseToMeshRotation(BasePose);

		// Add MeshSpaceRotAdditive to it
		FAnimationRuntime::AccumulateLocalSpaceAdditivePoseInternal(BasePose, MeshSpaceRotationAdditive, Weight);

		// Convert back to local space
		FAnimationRuntime::ConvertMeshRotationPoseToLocalSpace(BasePose);
	}
}

/** 
 * return ETypeAdvanceAnim type
 */
ETypeAdvanceAnim FAnimationRuntime::AdvanceTime(const bool& bAllowLooping, const float& MoveDelta, float& InOutTime, const float& EndTime)
{
	InOutTime += MoveDelta;

	if( InOutTime < 0.f || InOutTime > EndTime )
	{
		if( bAllowLooping )
		{
			if( EndTime != 0.f )
			{
				InOutTime	= FMath::Fmod(InOutTime, EndTime);
				// Fmod doesn't give result that falls into (0, EndTime), but one that falls into (-EndTime, EndTime). Negative values need to be handled in custom way
				if( InOutTime < 0.f )
				{
					InOutTime += EndTime;
				}
			}
			else
			{
				// end time is 0.f
				InOutTime = 0.f;
			}

			// it has been looped
			return ETAA_Looped;
		}
		else 
		{
			// If not, snap time to end of sequence and stop playing.
			InOutTime = FMath::Clamp(InOutTime, 0.f, EndTime);
			return ETAA_Finished;
		}
	}

	return ETAA_Default;
}

/** 
 * Scale transforms by Weight.
 * Result is obviously NOT normalized.
 */
void FAnimationRuntime::ApplyWeightToTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms, float Weight)
{
	const TArray<FBoneIndexType>& RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	ScalarRegister MultWeight(Weight);
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		Atoms[BoneIndex] *= MultWeight;
	}			
}

/* from % from OutKeyIndex1, meaning (CurrentKeyIndex(float)-OutKeyIndex1)/(OutKeyIndex2-OutKeyIndex1) */
void FAnimationRuntime::GetKeyIndicesFromTime(int32& OutKeyIndex1, int32& OutKeyIndex2, float& OutAlpha, const float Time, const int32 NumFrames, const float SequenceLength)
{
	// Check for 1-frame, before-first-frame and after-last-frame cases.
	if( Time <= 0.f || NumFrames == 1 )
	{
		OutKeyIndex1 = 0;
		OutKeyIndex2 = 0;
		OutAlpha = 0.f;
		return;
	}

	const int32 LastIndex		= NumFrames - 1;
	if( Time >= SequenceLength )
	{
		OutKeyIndex1 = LastIndex;
		OutKeyIndex2 = (OutKeyIndex1 + 1) % (NumFrames);
		OutAlpha = 0.f;
		return;
	}

	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	const int32 NumKeys = NumFrames - 1;
	const float KeyPos = ((float)NumKeys * Time) / SequenceLength;

	// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
	const int32 KeyIndex1 = FMath::Clamp<int32>( FMath::FloorToInt(KeyPos), 0, NumFrames-1 );  // @todo should be changed to FMath::TruncToInt

	// The alpha (fractional part) is then just the remainder.
	const float Alpha = KeyPos - (float)KeyIndex1;

	int32 KeyIndex2 = KeyIndex1 + 1;

	// If we have gone over the end, do different things in case of looping
	if( KeyIndex2 == NumFrames )
	{
		KeyIndex2 = KeyIndex1;
	}

	OutKeyIndex1 = KeyIndex1;
	OutKeyIndex2 = KeyIndex2;
	OutAlpha = Alpha;
}

FTransform FAnimationRuntime::GetComponentSpaceRefPose(const FCompactPoseBoneIndex& CompactPoseBoneIndex, const FBoneContainer& BoneContainer)
{
	FCompactPoseBoneIndex CurrentIndex = CompactPoseBoneIndex;
	FTransform CSTransform = FTransform::Identity;
	while (CurrentIndex.GetInt() != INDEX_NONE)
	{
		CSTransform *= BoneContainer.GetRefPoseTransform(CurrentIndex);
		CurrentIndex = BoneContainer.GetParentBoneIndex(CurrentIndex);
	}
	
	return CSTransform;
}

void FAnimationRuntime::FillWithRefPose(TArray<FTransform>& OutAtoms, const FBoneContainer& RequiredBones)
{
	// Copy Target Asset's ref pose.
	OutAtoms = RequiredBones.GetRefPoseArray();

	// If retargeting is disabled, copy ref pose from Skeleton, rather than mesh.
	// this is only used in editor and for debugging.
	if( RequiredBones.GetDisableRetargeting() )
	{
		checkSlow( RequiredBones.IsValid() );
		// Only do this if we have a mesh. otherwise we're not retargeting animations.
		if( RequiredBones.GetSkeletalMeshAsset() )
		{
			TArray<int32> const& PoseToSkeletonBoneIndexArray = RequiredBones.GetPoseToSkeletonBoneIndexArray();
			TArray<FBoneIndexType> const& RequireBonesIndexArray = RequiredBones.GetBoneIndicesArray();
			TArray<FTransform> const& SkeletonRefPose = RequiredBones.GetSkeletonAsset()->GetRefLocalPoses();

			for (int32 ArrayIndex = 0; ArrayIndex<RequireBonesIndexArray.Num(); ArrayIndex++)
			{
				int32 const& PoseBoneIndex = RequireBonesIndexArray[ArrayIndex];
				int32 const& SkeletonBoneIndex = PoseToSkeletonBoneIndexArray[PoseBoneIndex];

				// Pose bone index should always exist in Skeleton
				checkSlow(SkeletonBoneIndex != INDEX_NONE);
				OutAtoms[PoseBoneIndex] = SkeletonRefPose[SkeletonBoneIndex];
			}
		}
	}
}

#if WITH_EDITOR
void FAnimationRuntime::FillWithRetargetBaseRefPose(FCompactPose& OutPose, const USkeletalMesh* Mesh)
{
	// Copy Target Asset's ref pose.
	if (Mesh)
	{
		for (FCompactPoseBoneIndex BoneIndex : OutPose.ForEachBoneIndex())
		{
			int32 PoseIndex = OutPose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex).GetInt();
			if (Mesh->RetargetBasePose.IsValidIndex(PoseIndex))
			{
				OutPose[BoneIndex] = Mesh->RetargetBasePose[PoseIndex];
			}
		}
	}
}
#endif // WITH_EDITOR

void FAnimationRuntime::ConvertPoseToMeshSpace(const TArray<FTransform>& LocalTransforms, TArray<FTransform>& MeshSpaceTransforms, const FBoneContainer& RequiredBones)
{
	const int32 NumBones = RequiredBones.GetNumBones();

	// right now all this does is to convert to SpaceBases
	check( NumBones == LocalTransforms.Num() );
	check( NumBones == MeshSpaceTransforms.Num() );

	const FTransform* LocalTransformsData = LocalTransforms.GetData(); 
	FTransform* SpaceBasesData = MeshSpaceTransforms.GetData();
	const TArray<FBoneIndexType>& RequiredBoneIndexArray = RequiredBones.GetBoneIndicesArray();

	// First bone is always root bone, and it doesn't have a parent.
	{
		check( RequiredBoneIndexArray[0] == 0 );
		MeshSpaceTransforms[0] = LocalTransforms[0];
	}

	const int32 NumRequiredBones = RequiredBoneIndexArray.Num();
	for(int32 i=1; i<NumRequiredBones; i++)
	{
		const int32 BoneIndex = RequiredBoneIndexArray[i];
		FPlatformMisc::Prefetch(SpaceBasesData + BoneIndex);

		// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
		const int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
		FPlatformMisc::Prefetch(SpaceBasesData + ParentIndex);

		FTransform::Multiply(SpaceBasesData + BoneIndex, LocalTransformsData + BoneIndex, SpaceBasesData + ParentIndex);

		checkSlow( MeshSpaceTransforms[BoneIndex].IsRotationNormalized() );
		checkSlow( !MeshSpaceTransforms[BoneIndex].ContainsNaN() );
	}
}

/** 
 *	Utility for taking an array of bone indices and ensuring that all parents are present 
 *	(ie. all bones between those in the array and the root are present). 
 *	Note that this must ensure the invariant that parent occur before children in BoneIndices.
 */
void FAnimationRuntime::EnsureParentsPresent(TArray<FBoneIndexType>& BoneIndices, const FReferenceSkeleton& RefSkeleton )
{
	RefSkeleton.EnsureParentsExist(BoneIndices);
}

void FAnimationRuntime::ExcludeBonesWithNoParents(const TArray<int32>& BoneIndices, const FReferenceSkeleton& RefSkeleton, TArray<int32>& FilteredRequiredBones)
{
	// Filter list, we only want bones that have their parents present in this array.
	FilteredRequiredBones.Empty(BoneIndices.Num());

	for (int32 Index=0; Index<BoneIndices.Num(); Index++)
	{
		const int32& BoneIndex = BoneIndices[Index];
		// Always add root bone.
		if( BoneIndex == 0 )
		{
			FilteredRequiredBones.Add(BoneIndex);
		}
		else
		{
			const int32 ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
			if( FilteredRequiredBones.Contains(ParentBoneIndex) )
			{
				FilteredRequiredBones.Add(BoneIndex);
			}
			else
			{
				UE_LOG(LogAnimation, Warning, TEXT("ExcludeBonesWithNoParents: Filtering out bone (%s) since parent (%s) is missing"), 
					*RefSkeleton.GetBoneName(BoneIndex).ToString(), *RefSkeleton.GetBoneName(ParentBoneIndex).ToString());
			}
		}
	}
}

void FAnimationRuntime::BlendMeshPosesPerBoneWeights(
		struct FCompactPose& BasePose,
		const TArray<struct FCompactPose>& BlendPoses,
		struct FBlendedCurve& BaseCurve,
		const TArray<struct FBlendedCurve>& BlendedCurves,
		const TArray<FPerBoneBlendWeight>& BoneBlendWeights,
		ECurveBlendOption::Type CurveBlendOption,
		/*out*/ FCompactPose& OutPose,
		/*out*/ struct FBlendedCurve& OutCurve)
{
	const int32 NumBones = BasePose.GetNumBones();
	check(BoneBlendWeights.Num() == NumBones);
	check(OutPose.GetNumBones() == NumBones);

	const int32 NumPoses = BlendPoses.Num();
	for (const FPerBoneBlendWeight& PerBoneBlendWeight : BoneBlendWeights)
	{
		check(PerBoneBlendWeight.SourceIndex >= 0);
		check(PerBoneBlendWeight.SourceIndex < NumPoses);
	}

	for (const FCompactPose& BlendPose : BlendPoses)
	{
		check(BlendPose.GetNumBones() == NumBones);
	}

	const FBoneContainer& BoneContainer = BasePose.GetBoneContainer();

	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> SourceRotations;
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> BlendRotations;
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> TargetRotations;

	SourceRotations.AddUninitialized(NumBones);
	BlendRotations.AddUninitialized(NumBones);
	TargetRotations.AddUninitialized(NumBones);

	TArray<float> MaxPoseWeights;
	MaxPoseWeights.AddZeroed(NumPoses);

	for (const FCompactPoseBoneIndex BoneIndex : BasePose.ForEachBoneIndex())
	{
		const int32 PoseIndex = BoneBlendWeights[BoneIndex.GetInt()].SourceIndex;
		const FCompactPoseBoneIndex ParentIndex = BoneContainer.GetParentBoneIndex(BoneIndex);

		FQuat SrcRotationInMesh;
		FQuat TargetRotationInMesh;

		if (ParentIndex != INDEX_NONE)
		{
			SrcRotationInMesh = SourceRotations[ParentIndex] * BasePose[BoneIndex].GetRotation();
			TargetRotationInMesh = TargetRotations[ParentIndex] * BlendPoses[PoseIndex][BoneIndex].GetRotation();
		}
		else
		{
			SrcRotationInMesh = BasePose[BoneIndex].GetRotation();
			TargetRotationInMesh = BlendPoses[PoseIndex][BoneIndex].GetRotation();
		}

		// update mesh based rotations
		SourceRotations[BoneIndex] = SrcRotationInMesh;
		TargetRotations[BoneIndex] = TargetRotationInMesh;

		// now update outer
		FTransform BaseAtom = BasePose[BoneIndex];
		FTransform TargetAtom = BlendPoses[PoseIndex][BoneIndex];
		FTransform BlendAtom;

		const float BlendWeight = FMath::Clamp(BoneBlendWeights[BoneIndex.GetInt()].BlendWeight, 0.f, 1.f);
		MaxPoseWeights[PoseIndex] = FMath::Max(MaxPoseWeights[PoseIndex], BlendWeight);

		if (!FAnimWeight::IsRelevant(BlendWeight))
		{
			BlendAtom = BaseAtom;
			BlendRotations[BoneIndex] = SourceRotations[BoneIndex];
		}
		else if (FAnimWeight::IsFullWeight(BlendWeight))
		{
			BlendAtom = TargetAtom;
			BlendRotations[BoneIndex] = TargetRotations[BoneIndex];
		}
		else // we want blend here
		{
			BlendAtom = BaseAtom;
			BlendAtom.BlendWith(TargetAtom, BlendWeight);

			// blend rotation in mesh space
			BlendRotations[BoneIndex] = FQuat::FastLerp(SourceRotations[BoneIndex], TargetRotations[BoneIndex], BlendWeight);

			// Fast lerp produces un-normalized quaternions, re-normalize.
			BlendRotations[BoneIndex].Normalize();
		}

		OutPose[BoneIndex] = BlendAtom;
		if (ParentIndex != INDEX_NONE)
		{
			FQuat LocalBlendQuat = BlendRotations[ParentIndex].Inverse() * BlendRotations[BoneIndex];

			// local -> mesh -> local transformations can cause loss of precision for long bone chains, we have to normalize rotation there.
			LocalBlendQuat.Normalize();
			OutPose[BoneIndex].SetRotation(LocalBlendQuat);
		}
	}

	// time to blend curves
	// the way we blend curve per bone
	// is to find out max weight per that pose, and then apply that weight to the curve
	{
		TArray<const FBlendedCurve*> SourceCurves;
		TArray<float> SourceWegihts;

		SourceCurves.SetNumUninitialized(NumPoses+1);
		SourceWegihts.SetNumUninitialized(NumPoses +1);

		SourceCurves[0] = &BaseCurve;
		SourceWegihts[0] = 1.f;

		for(int32 Idx=0; Idx<NumPoses; ++Idx)
		{
			SourceCurves[Idx+1] = &BlendedCurves[Idx];
			SourceWegihts[Idx+1] = MaxPoseWeights[Idx];
		}

		BlendCurves(SourceCurves, SourceWegihts, OutCurve, CurveBlendOption);
	}
}

void FAnimationRuntime::BlendLocalPosesPerBoneWeights(
	FCompactPose& BasePose,
	const TArray<FCompactPose>& BlendPoses,
	struct FBlendedCurve& BaseCurve,
	const TArray<struct FBlendedCurve>& BlendedCurves,
	const TArray<FPerBoneBlendWeight>& BoneBlendWeights,
	ECurveBlendOption::Type CurveBlendOption,
	/*out*/ FCompactPose& OutPose, 
	/*out*/ struct FBlendedCurve& OutCurve)
{
	const int32 NumBones = BasePose.GetNumBones();
	check(BoneBlendWeights.Num() == NumBones);
	check(OutPose.GetNumBones() == NumBones);

	const int32 NumPoses = BlendPoses.Num();
	for (const FPerBoneBlendWeight& PerBoneBlendWeight : BoneBlendWeights)
	{
		check(PerBoneBlendWeight.SourceIndex >= 0);
		check(PerBoneBlendWeight.SourceIndex < NumPoses);
	}

	for (const FCompactPose& BlendPose : BlendPoses)
	{
		check(BlendPose.GetNumBones() == NumBones);
	}

	TArray<float> MaxPoseWeights;
	MaxPoseWeights.AddZeroed(NumPoses);

	for (FCompactPoseBoneIndex BoneIndex : BasePose.ForEachBoneIndex())
	{
		const int32 PoseIndex = BoneBlendWeights[BoneIndex.GetInt()].SourceIndex;
		const FTransform& BaseAtom = BasePose[BoneIndex];

		const float BlendWeight = FMath::Clamp(BoneBlendWeights[BoneIndex.GetInt()].BlendWeight, 0.f, 1.f);
		MaxPoseWeights[PoseIndex] = FMath::Max(MaxPoseWeights[PoseIndex], BlendWeight);

		if (!FAnimWeight::IsRelevant(BlendWeight))
		{
			OutPose[BoneIndex] = BaseAtom;
		}
		else if (FAnimWeight::IsFullWeight(BlendWeight))
		{
			OutPose[BoneIndex] = BlendPoses[PoseIndex][BoneIndex];
		}
		else // we want blend here
		{
			FTransform BlendAtom = BaseAtom;
			const FTransform& TargetAtom = BlendPoses[PoseIndex][BoneIndex];
			BlendAtom.BlendWith(TargetAtom, BlendWeight);
			OutPose[BoneIndex] = BlendAtom;
		}
	}

	// time to blend curves
	// the way we blend curve per bone
	// is to find out max weight per that pose, and then apply that weight to the curve
	{
		TArray<const FBlendedCurve*> SourceCurves;
		TArray<float> SourceWegihts;

		SourceCurves.SetNumUninitialized(NumPoses +1);
		SourceWegihts.SetNumUninitialized(NumPoses +1);

		SourceCurves[0] = &BaseCurve;
		SourceWegihts[0] = 1.f;

		for (int32 Idx=0; Idx<NumPoses; ++Idx)
		{
			SourceCurves[Idx+1] = &BlendedCurves[Idx];
			SourceWegihts[Idx+1] = MaxPoseWeights[Idx];
		}
		
		BlendCurves(SourceCurves, SourceWegihts, OutCurve, CurveBlendOption);
	}
}

void FAnimationRuntime::UpdateDesiredBoneWeight(const TArray<FPerBoneBlendWeight>& SrcBoneBlendWeights, TArray<FPerBoneBlendWeight>& TargetBoneBlendWeights, const TArray<float>& BlendWeights)
{
	// in the future, cache this outside
	ensure (TargetBoneBlendWeights.Num() == SrcBoneBlendWeights.Num());

	FMemory::Memzero(TargetBoneBlendWeights.GetData(), TargetBoneBlendWeights.Num() * sizeof(FPerBoneBlendWeight));

	for (int32 BoneIndex = 0; BoneIndex < SrcBoneBlendWeights.Num(); ++BoneIndex)
	{
		const int32 PoseIndex = SrcBoneBlendWeights[BoneIndex].SourceIndex;
		check(PoseIndex < BlendWeights.Num());
		float TargetBlendWeight = BlendWeights[PoseIndex] * SrcBoneBlendWeights[BoneIndex].BlendWeight;
		
		// if relevant, otherwise all initialized as zero
		if (FAnimWeight::IsRelevant(TargetBlendWeight))
		{
			TargetBoneBlendWeights[BoneIndex].SourceIndex = PoseIndex;
			TargetBoneBlendWeights[BoneIndex].BlendWeight = TargetBlendWeight;
		}
	}
}

void FAnimationRuntime::BlendPosesPerBoneFilter(
	struct FCompactPose& BasePose, 
	const TArray<struct FCompactPose>& BlendPoses, 
	struct FBlendedCurve& BaseCurve, 
	const TArray<struct FBlendedCurve>& BlendedCurves, 
	struct FCompactPose& OutPose, 
	struct FBlendedCurve& OutCurve, 
	TArray<FPerBoneBlendWeight>& BoneBlendWeights, 
	bool bMeshSpaceRotationBlending, 
	ECurveBlendOption::Type CurveBlendOption)
{
	SCOPE_CYCLE_COUNTER(STAT_BlendPosesPerBoneFilter);

	ensure(OutPose.GetNumBones() == BasePose.GetNumBones());

	if (BlendPoses.Num() != 0)
	{
		if (bMeshSpaceRotationBlending)
		{
			BlendMeshPosesPerBoneWeights(BasePose, BlendPoses, BaseCurve, BlendedCurves, BoneBlendWeights, CurveBlendOption, OutPose, OutCurve);
		}
		else
		{
			BlendLocalPosesPerBoneWeights(BasePose, BlendPoses, BaseCurve, BlendedCurves, BoneBlendWeights, CurveBlendOption, OutPose, OutCurve);
		}
	}
	else // if no blendpose, outpose = basepose
	{
		OutPose = BasePose;
	}
}

void FAnimationRuntime::CreateMaskWeights(TArray<FPerBoneBlendWeight>& BoneBlendWeights, const TArray<FInputBlendPose>& BlendFilters, const USkeleton* Skeleton)
{
	if ( Skeleton )
	{
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		
		const int32 NumBones = RefSkeleton.GetNum();
		BoneBlendWeights.Reset(NumBones);
		BoneBlendWeights.AddZeroed(NumBones);

		// base mask bone
		for (int32 PoseIndex=0; PoseIndex<BlendFilters.Num(); ++PoseIndex)
		{
			const FInputBlendPose& BlendPose = BlendFilters[PoseIndex];

			for (int32 BranchIndex=0; BranchIndex<BlendPose.BranchFilters.Num(); ++BranchIndex)
			{
				const FBranchFilter& BranchFilter = BlendPose.BranchFilters[BranchIndex];
				const int32 MaskBoneIndex = RefSkeleton.FindBoneIndex(BranchFilter.BoneName);

				if (MaskBoneIndex != INDEX_NONE)
				{
					// how much weight increase Per depth
					const float IncreaseWeightPerDepth = (BranchFilter.BlendDepth != 0) ? (1.f/((float)BranchFilter.BlendDepth)) : 1.f;

					// go through skeleton bone hierarchy.
					// Bones are ordered, parents before children. So we can start looking at MaskBoneIndex for children.
					for (int32 BoneIndex = MaskBoneIndex; BoneIndex < NumBones; ++BoneIndex)
					{
						// if Depth == -1, it's not a child
						const int32 Depth = RefSkeleton.GetDepthBetweenBones(BoneIndex, MaskBoneIndex);
						if (Depth != -1)
						{
							// when you write to buffer, you'll need to match with BasePoses BoneIndex
							FPerBoneBlendWeight& BoneBlendWeight = BoneBlendWeights[BoneIndex];

							BoneBlendWeight.SourceIndex = PoseIndex;
							const float BlendIncrease = IncreaseWeightPerDepth * (float)(Depth + 1);
							BoneBlendWeight.BlendWeight = FMath::Clamp<float>(BoneBlendWeight.BlendWeight + BlendIncrease, 0.f, 1.f);
						}
					}
				}
			}
		}
	}
}

void FAnimationRuntime::ConvertCSTransformToBoneSpace(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, FTransform& InOutCSBoneTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space)
{
	ConvertCSTransformToBoneSpace(SkelComp->GetComponentTransform(), MeshBases, InOutCSBoneTM, BoneIndex, Space);
}

void FAnimationRuntime::ConvertCSTransformToBoneSpace(const FTransform& ComponentTransform, FCSPose<FCompactPose>& MeshBases, FTransform& InOutCSBoneTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space)
{
	switch( Space )
	{
		case BCS_WorldSpace : 
			// world space, so component space * component to world
			InOutCSBoneTM *= ComponentTransform;
			break;

		case BCS_ComponentSpace :
			// Component Space, no change.
			break;

		case BCS_ParentBoneSpace :
			{
				const FCompactPoseBoneIndex ParentIndex = MeshBases.GetPose().GetParentBoneIndex(BoneIndex);
				if (ParentIndex != INDEX_NONE)
				{
					const FTransform& ParentTM = MeshBases.GetComponentSpaceTransform(ParentIndex);
					InOutCSBoneTM.SetToRelativeTransform(ParentTM);
				}
			}
			break;

		case BCS_BoneSpace :
			{
				const FTransform& BoneTM = MeshBases.GetComponentSpaceTransform(BoneIndex);
				InOutCSBoneTM.SetToRelativeTransform(BoneTM);
			}
			break;

		default:
			UE_LOG(LogAnimation, Warning, TEXT("ConvertCSTransformToBoneSpace: Unknown BoneSpace %d"), (int32)Space);
			break;
	}
}

void FAnimationRuntime::ConvertBoneSpaceTransformToCS(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, FTransform& InOutBoneSpaceTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space)
{
	ConvertBoneSpaceTransformToCS(SkelComp->GetComponentTransform(), MeshBases, InOutBoneSpaceTM, BoneIndex, Space);
}

void FAnimationRuntime::ConvertBoneSpaceTransformToCS(const FTransform& ComponentTransform, FCSPose<FCompactPose>& MeshBases, FTransform& InOutBoneSpaceTM, FCompactPoseBoneIndex BoneIndex, EBoneControlSpace Space)
{
	switch( Space )
	{
		case BCS_WorldSpace : 
			InOutBoneSpaceTM.SetToRelativeTransform(ComponentTransform);
			break;

		case BCS_ComponentSpace :
			// Component Space, no change.
			break;

		case BCS_ParentBoneSpace :
			if( BoneIndex != INDEX_NONE )
			{
				const FCompactPoseBoneIndex ParentIndex = MeshBases.GetPose().GetParentBoneIndex(BoneIndex);
				if( ParentIndex != INDEX_NONE )
				{
					const FTransform& ParentTM = MeshBases.GetComponentSpaceTransform(ParentIndex);
					InOutBoneSpaceTM *= ParentTM;
				}
			}
			break;

		case BCS_BoneSpace :
			if( BoneIndex != INDEX_NONE )
			{
				const FTransform& BoneTM = MeshBases.GetComponentSpaceTransform(BoneIndex);
				InOutBoneSpaceTM *= BoneTM;
			}
			break;

		default:
			UE_LOG(LogAnimation, Warning, TEXT("ConvertBoneSpaceTransformToCS: Unknown BoneSpace %d"), (int32)Space);
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/// Pose conversion functions
////////////////////////////////////////////////////////////////////////////////////////////////////

FTransform FAnimationRuntime::GetSpaceTransform(FA2Pose& Pose, int32 Index)
{
	return Pose.Bones[Index];
}

FTransform FAnimationRuntime::GetSpaceTransform(FA2CSPose& Pose, int32 Index)
{
	return Pose.GetComponentSpaceTransform(Index);
}

void FAnimationRuntime::SetSpaceTransform(FA2Pose& Pose, int32 Index, FTransform& NewTransform)
{
	Pose.Bones[Index] = NewTransform;
}

void FAnimationRuntime::SetSpaceTransform(FA2CSPose& Pose, int32 Index, FTransform& NewTransform)
{
	Pose.SetComponentSpaceTransform(Index, NewTransform);
}

void FAnimationRuntime::TickBlendWeight(float DeltaTime, float DesiredWeight, float& Weight, float& BlendTime)
{
	// if it's not same, we'll need to update weight
	if (DesiredWeight != Weight)
	{
		if (BlendTime == 0.f)
		{
			// no blending, just go
			Weight = DesiredWeight;
		}
		else
		{
			float WeightChangePerTime = (DesiredWeight-Weight)/BlendTime;
			Weight += WeightChangePerTime*DeltaTime;

			// going up or down, changes where to clamp to 
			if (WeightChangePerTime > 0.f)
			{
				Weight = FMath::Clamp<float>(Weight, 0.f, DesiredWeight);
			}
			else // if going down
			{
				Weight = FMath::Clamp<float>(Weight, DesiredWeight, 1.f);
			}

			BlendTime-=DeltaTime;
		}
	}
}

#if DO_GUARD_SLOW
// use checkSlow to use this function for debugging
bool FAnimationRuntime::ContainsNaN(TArray<FBoneIndexType>& RequiredBoneIndices, FA2Pose& Pose) 
{
	for (int32 Iter = 0; Iter < RequiredBoneIndices.Num(); ++Iter)
	{
		const int32 BoneIndex = RequiredBoneIndices[Iter];
		if (Pose.Bones[BoneIndex].ContainsNaN())
		{
			return true;
		}
	}

	return false;
}
#endif

FTransform FAnimationRuntime::GetComponentSpaceTransform(const FReferenceSkeleton& RefSkeleton, const TArray<FTransform> &BoneSpaceTransforms, int32 BoneIndex)
{
	if (RefSkeleton.IsValidIndex(BoneIndex))
	{
		// initialize to identity since some of them don't have tracks
		int32 IterBoneIndex = BoneIndex;
		FTransform CompTransform = BoneSpaceTransforms[BoneIndex];

		do
		{
			int32 ParentIndex = RefSkeleton.GetParentIndex(IterBoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				CompTransform = CompTransform * BoneSpaceTransforms[ParentIndex];
			}

			IterBoneIndex = ParentIndex;
		} while (RefSkeleton.IsValidIndex(IterBoneIndex));

		return CompTransform;
	}

	return FTransform::Identity;
}

FTransform FAnimationRuntime::GetComponentSpaceTransformRefPose(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
{
	return GetComponentSpaceTransform(RefSkeleton, RefSkeleton.GetRefBonePose(), BoneIndex);
}

void FAnimationRuntime::FillUpComponentSpaceTransforms(const FReferenceSkeleton& RefSkeleton, const TArray<FTransform> &BoneSpaceTransforms, TArray<FTransform> &ComponentSpaceTransforms)
{
	ComponentSpaceTransforms.Empty(BoneSpaceTransforms.Num());
	ComponentSpaceTransforms.AddUninitialized(BoneSpaceTransforms.Num());

	// initialize to identity since some of them don't have tracks
	for (int Index = 0; Index < ComponentSpaceTransforms.Num(); ++Index)
	{
		int32 ParentIndex = RefSkeleton.GetParentIndex(Index);
		if (ParentIndex != INDEX_NONE)
		{
			ComponentSpaceTransforms[Index] = BoneSpaceTransforms[Index] * ComponentSpaceTransforms[ParentIndex];
		}
		else
		{
			ComponentSpaceTransforms[Index] = BoneSpaceTransforms[Index];
		}
	}
}

#if WITH_EDITOR
void FAnimationRuntime::FillUpComponentSpaceTransformsRefPose(const USkeleton* Skeleton, TArray<FTransform> &ComponentSpaceTransforms)
{
	check(Skeleton);

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const TArray<FTransform>& ReferencePose = RefSkeleton.GetRefBonePose();
	FillUpComponentSpaceTransforms(RefSkeleton, ReferencePose, ComponentSpaceTransforms);
}

void FAnimationRuntime::FillUpComponentSpaceTransformsRetargetBasePose(const USkeleton* Skeleton, TArray<FTransform> &ComponentSpaceTransforms)
{
	check(Skeleton);

	// @Todo fixme: this has to get preview mesh instead of skeleton
	const USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh();
	if (PreviewMesh)
	{
		const TArray<FTransform>& ReferencePose = PreviewMesh->RetargetBasePose;
		const FReferenceSkeleton& RefSkeleton = PreviewMesh->RefSkeleton;
		FillUpComponentSpaceTransforms(RefSkeleton, ReferencePose, ComponentSpaceTransforms);
	}
	else
	{
		FAnimationRuntime::FillUpComponentSpaceTransformsRefPose(Skeleton, ComponentSpaceTransforms);
	}
}
#endif // WITH_EDITOR

/** See if an array of ActiveMorphTargets already contains the supplied anim */
static int32 FindMorphTarget(const TArray<FActiveMorphTarget>& ActiveMorphTargets, UMorphTarget* InMorphTarget)
{
	for(int32 i=0; i<ActiveMorphTargets.Num(); i++)
	{
		if(ActiveMorphTargets[i].MorphTarget == InMorphTarget)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void FAnimationRuntime::AppendActiveMorphTargets(const USkeletalMesh* InSkeletalMesh, const TMap<FName, float>& MorphCurveAnims, TArray<FActiveMorphTarget>& InOutActiveMorphTargets, TArray<float>& InOutMorphTargetWeights)
{
	if (!InSkeletalMesh)
	{
		return;
	}

	// Then go over the CurveKeys finding morph targets by name
	for(auto CurveIter=MorphCurveAnims.CreateConstIterator(); CurveIter; ++CurveIter)
	{
		const FName& CurveName	= (CurveIter).Key();
		const float Weight	= (CurveIter).Value();

		// ensure the buffer fits the size

		// @note that this only adds zero buffer if it doesn't have enough buffer with the correct size and that is intended
		// there is three places to resize this buffer
		//
		// one is init anim, where we initialize the buffer first time. We need this so that if you don't call Tick, it can have buffer assigned for renderer to get
		// second is tick component, where we make sure the buffer size is correct. We need that so that if you don't have animation or your morphtarget buffer size changes, we want to make sure that buffer is set correctly
		// third is this place where the buffer really matters for game thread, we need to resize if needed in case morphtarget is deleted or added. 
		// the reason you need this is because some other places calling append buffer without going through proper tick component - for example, calling TickAnimation directly
		//
		// if somehow it gets rendered without going through these places, there will be crash. Renderer expect the buffer size being same. 
		InOutMorphTargetWeights.SetNumZeroed(InSkeletalMesh->MorphTargets.Num());

		// Find morph reference
		int32 SkeletalMorphIndex = INDEX_NONE;
		UMorphTarget* Target = InSkeletalMesh->FindMorphTargetAndIndex(CurveName, SkeletalMorphIndex);
		if (Target != nullptr)
		{
			// If it has a valid weight
			if (FMath::Abs(Weight) > MinMorphTargetBlendWeight)
			{
				// See if this morph target already has an entry
				int32 MorphIndex = FindMorphTarget(InOutActiveMorphTargets, Target);
				// If not, add it
				if (MorphIndex == INDEX_NONE)
				{
					InOutActiveMorphTargets.Add(FActiveMorphTarget(Target, SkeletalMorphIndex));
					InOutMorphTargetWeights[SkeletalMorphIndex] = Weight;
				}
				else
				{
					// If it does, use the max weight
					check(SkeletalMorphIndex == InOutActiveMorphTargets[MorphIndex].WeightIndex);
					InOutMorphTargetWeights[SkeletalMorphIndex] = Weight;
				}
			}
			else
			{
				int32 MorphIndex = FindMorphTarget(InOutActiveMorphTargets, Target);
				if (MorphIndex != INDEX_NONE)
				{
					// clear weight
					InOutMorphTargetWeights[SkeletalMorphIndex] = 0.f;
				}
			}

		}
	}
}

int32 FAnimationRuntime::GetStringDistance(const FString& First, const FString& Second) 
{
	// Finds the distance between strings, where the distance is the number of operations we would need
	// to perform on First to match Second.
	// Operations are: Adding a character, Removing a character, changing a character.

	const int32 FirstLength = First.Len();
	const int32 SecondLength = Second.Len();

	// Already matching
	if (First == Second)
	{
		return 0;
	}

	// No first string, so we need to add SecondLength characters to match
	if (FirstLength == 0)
	{
		return SecondLength;
	}

	// No Second string, so we need to add FirstLength characters to match
	if (SecondLength == 0)
	{
		return FirstLength;
	}

	TArray<int32> PrevRow;
	TArray<int32> NextRow;
	PrevRow.AddZeroed(SecondLength + 1);
	NextRow.AddZeroed(SecondLength + 1);

	// Initialise prev row to num characters we need to remove from Second
	for (int32 I = 0; I < PrevRow.Num(); ++I)
	{
		PrevRow[I] = I;
	}

	for (int32 I = 0; I < FirstLength; ++I)
	{
		// Calculate current row
		NextRow[0] = I + 1;

		for (int32 J = 0; J < SecondLength; ++J)
		{
			int32 Indicator = (First[I] == Second[J]) ? 0 : 1;
			NextRow[J + 1] = FMath::Min3(NextRow[J] + 1, PrevRow[J + 1] + 1, PrevRow[J] + Indicator);
		}

		// Copy back
		PrevRow = NextRow;
	}

	return NextRow[SecondLength];
}

void FAnimationRuntime::RetargetBoneTransform(const USkeleton* MySkeleton, const FName& RetargetSource, FTransform& BoneTransform, const int32 SkeletonBoneIndex, const FCompactPoseBoneIndex& BoneIndex, const FBoneContainer& RequiredBones, const bool bIsBakedAdditive)
{
	if (MySkeleton)
	{
		switch (MySkeleton->GetBoneTranslationRetargetingMode(SkeletonBoneIndex))
		{
		case EBoneTranslationRetargetingMode::AnimationScaled:
		{
			// @todo - precache that in FBoneContainer when we have SkeletonIndex->TrackIndex mapping. So we can just apply scale right away.
			const TArray<FTransform>& SkeletonRefPoseArray = MySkeleton->GetRefLocalPoses(RetargetSource);
			const float SourceTranslationLength = SkeletonRefPoseArray[SkeletonBoneIndex].GetTranslation().Size();
			if (SourceTranslationLength > KINDA_SMALL_NUMBER)
			{
				const float TargetTranslationLength = RequiredBones.GetRefPoseTransform(BoneIndex).GetTranslation().Size();
				BoneTransform.ScaleTranslation(TargetTranslationLength / SourceTranslationLength);
			}
			break;
		}

		case EBoneTranslationRetargetingMode::Skeleton:
		{
			BoneTransform.SetTranslation(bIsBakedAdditive ? FVector::ZeroVector : RequiredBones.GetRefPoseTransform(BoneIndex).GetTranslation());
			break;
		}


		case EBoneTranslationRetargetingMode::AnimationRelative:
		{
			// With baked additive animations, Animation Relative delta gets canceled out, so we can skip it.
			// (A1 + Rel) - (A2 + Rel) = A1 - A2.
			if (!bIsBakedAdditive)
			{
				const TArray<FTransform>& AuthoredOnRefSkeleton = MySkeleton->GetRefLocalPoses(RetargetSource);
				const TArray<FTransform>& PlayingOnRefSkeleton = RequiredBones.GetRefPoseArray();

				const FTransform& RefPoseTransform = RequiredBones.GetRefPoseTransform(BoneIndex);

				// Apply the retargeting as if it were an additive difference between the current skeleton and the retarget skeleton. 
				BoneTransform.SetRotation(BoneTransform.GetRotation() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetRotation().Inverse() * RefPoseTransform.GetRotation());
				BoneTransform.SetTranslation(BoneTransform.GetTranslation() + (RefPoseTransform.GetTranslation() - AuthoredOnRefSkeleton[SkeletonBoneIndex].GetTranslation()));
				BoneTransform.SetScale3D(BoneTransform.GetScale3D() * (RefPoseTransform.GetScale3D() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetSafeScaleReciprocal(AuthoredOnRefSkeleton[SkeletonBoneIndex].GetScale3D())));
				BoneTransform.NormalizeRotation();
			}
			break;
		}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// FA2CSPose
/////////////////////////////////////////////////////////////////////////////////////////

/** constructor - needs LocalPoses **/
void FA2CSPose::AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FA2Pose& LocalPose)
{
	AllocateLocalPoses(InBoneContainer, LocalPose.Bones);
}

void FA2CSPose::AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FTransformArrayA2& LocalBones)
{
	check( InBoneContainer.IsValid() );
	BoneContainer = &InBoneContainer;

	Bones = LocalBones;
	ComponentSpaceFlags.Init(0, Bones.Num());

	// root is same, so set root first
	check(ComponentSpaceFlags.Num() > 0);
	ComponentSpaceFlags[0] = 1;
}

bool FA2CSPose::IsValid() const
{
	return (BoneContainer && BoneContainer->IsValid());
}

int32 FA2CSPose::GetParentBoneIndex(const int32 BoneIndex) const
{
	checkSlow( IsValid() );
	return BoneContainer->GetParentBoneIndex(BoneIndex);
}

/** Do not access Bones array directly but via this 
 * This will fill up gradually mesh space bases 
 */
FTransform FA2CSPose::GetComponentSpaceTransform(int32 BoneIndex)
{
	check(Bones.IsValidIndex(BoneIndex));

	// if not evaluate, calculate it
	if( ComponentSpaceFlags[BoneIndex] == 0 )
	{
		CalculateComponentSpaceTransform(BoneIndex);
	}

	return Bones[BoneIndex];
}

void FA2CSPose::SetComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	check (Bones.IsValidIndex(BoneIndex));

	// this one forcefully sets component space transform
	Bones[BoneIndex] = NewTransform;
	ComponentSpaceFlags[BoneIndex] = 1;
}

/**
 * Convert Bone to Local Space.
 */
void FA2CSPose::ConvertBoneToLocalSpace(int32 BoneIndex)
{
	checkSlow( IsValid() );

	// If BoneTransform is in Component Space, then convert it.
	// Never convert Root to Local Space.
	if( BoneIndex > 0 && ComponentSpaceFlags[BoneIndex] == 1 )
	{
		const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);

		// Verify that our Parent is also in Component Space. That should always be the case.
		check( ComponentSpaceFlags[ParentIndex] == 1 );

		// Convert to local space.
		Bones[BoneIndex].SetToRelativeTransform( Bones[ParentIndex] );
		ComponentSpaceFlags[BoneIndex] = 0;
	}
}

/** 
 * Do not access Bones array directly but via this 
 * This will fill up gradually mesh space bases 
 */
FTransform FA2CSPose::GetLocalSpaceTransform(int32 BoneIndex)
{
	check( Bones.IsValidIndex(BoneIndex) );
	checkSlow( IsValid() );

	// if evaluated, calculate it
	if( ComponentSpaceFlags[BoneIndex] )
	{
		const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			const FTransform ParentTransform = GetComponentSpaceTransform(ParentIndex);
			const FTransform& BoneTransform = Bones[BoneIndex];
			// calculate local space
			return BoneTransform.GetRelativeTransform(ParentTransform);
		}
	}

	return Bones[BoneIndex];
}

void FA2CSPose::SetLocalSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	check (Bones.IsValidIndex(BoneIndex));

	// this one forcefully sets component space transform
	Bones[BoneIndex] = NewTransform;
	ComponentSpaceFlags[BoneIndex] = 0;
}

/** Calculate all transform till parent **/
void FA2CSPose::CalculateComponentSpaceTransform(int32 BoneIndex)
{
	check( ComponentSpaceFlags[BoneIndex] == 0 );
	checkSlow( IsValid() );

	// root is already verified, so root should not come here
	// check AllocateLocalPoses
	const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);

	// if Parent already has been calculated, use it
	if( ComponentSpaceFlags[ParentIndex] == 0 )
	{
		// if Parent hasn't been calculated, also calculate parents
		CalculateComponentSpaceTransform(ParentIndex);
	}

	// current Bones(Index) should contain LocalPoses.
	Bones[BoneIndex] = Bones[BoneIndex] * Bones[ParentIndex];
	Bones[BoneIndex].NormalizeRotation();
	ComponentSpaceFlags[BoneIndex] = 1;
}

void FA2CSPose::ConvertToLocalPoses(FA2Pose& LocalPoses)  const
{
	checkSlow(IsValid());
	LocalPoses.Bones = Bones;

	// now we need to convert back to local bases
	// only convert back that has been converted to mesh base
	// if it was local base, and if it hasn't been modified
	// that is still okay even if parent is changed, 
	// that doesn't mean this local has to change
	// go from child to parent since I need parent inverse to go back to local
	// root is same, so no need to do Index == 0
	for(int32 BoneIndex=ComponentSpaceFlags.Num()-1; BoneIndex>0; --BoneIndex)
	{
		// root is already verified, so root should not come here
		// check AllocateLocalPoses
		const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);

		// convert back 
		if( ComponentSpaceFlags[BoneIndex] )
		{
			LocalPoses.Bones[BoneIndex].SetToRelativeTransform( LocalPoses.Bones[ParentIndex] );
			LocalPoses.Bones[BoneIndex].NormalizeRotation();
		}
	}
}

