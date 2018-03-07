// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

#define DEFAULT_SOURCEINDEX 0xFF
/////////////////////////////////////////////////////
// FAnimNode_LayeredBoneBlend

void FAnimNode_LayeredBoneBlend::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	const int NumPoses = BlendPoses.Num();
	checkSlow(BlendWeights.Num() == NumPoses);

	// initialize children
	BasePose.Initialize(Context);

	if (NumPoses > 0)
	{
		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			BlendPoses[ChildIndex].Initialize(Context);
		}

		// initialize mask weight now
		check (Context.AnimInstanceProxy->GetSkeleton());
		ReinitializeBoneBlendWeights(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
	}
}

#if WITH_EDITOR
void FAnimNode_LayeredBoneBlend::PostCompile(const class USkeleton* InSkeleton)
{
	FAnimNode_Base::PostCompile(InSkeleton);
	RebuildCacheData(InSkeleton);
}
#endif // WITH_EDITOR

void FAnimNode_LayeredBoneBlend::RebuildCacheData(const USkeleton* InSkeleton)
{
	// make sure it's Finished post load yet, this might cause more things to cache in initialize instead of during cooking
	if (InSkeleton && !(InSkeleton->GetFlags() & RF_NeedPostLoad))
	{
		FAnimationRuntime::CreateMaskWeights(PerBoneBlendWeights, LayerSetup, InSkeleton);
		SkeletonGuid = InSkeleton->GetGuid();
		VirtualBoneGuid = InSkeleton->GetVirtualBoneGuid();
	}
}

bool FAnimNode_LayeredBoneBlend::IsCacheInvalid(const USkeleton* InSkeleton) const
{
	return (InSkeleton->GetGuid() != SkeletonGuid || InSkeleton->GetVirtualBoneGuid() != VirtualBoneGuid);
}

void FAnimNode_LayeredBoneBlend::ReinitializeBoneBlendWeights(const FBoneContainer& RequiredBones, const USkeleton* Skeleton)
{
	if (IsCacheInvalid(Skeleton))
	{
		RebuildCacheData(Skeleton);
	}
	
	// build desired bone weights
	const TArray<FBoneIndexType>& RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	const int32 NumRequiredBones = RequiredBoneIndices.Num();
	DesiredBoneBlendWeights.SetNumZeroed(NumRequiredBones);
	for (int32 RequiredBoneIndex=0; RequiredBoneIndex<NumRequiredBones; RequiredBoneIndex++)
	{
		const int32 SkeletonBoneIndex = RequiredBones.GetSkeletonIndex(FCompactPoseBoneIndex(RequiredBoneIndex));
		DesiredBoneBlendWeights[RequiredBoneIndex] = PerBoneBlendWeights[SkeletonBoneIndex];
	}
	
	CurrentBoneBlendWeights.Reset(DesiredBoneBlendWeights.Num());
	CurrentBoneBlendWeights.AddZeroed(DesiredBoneBlendWeights.Num());

	//Reinitialize bone blend weights now that we have cleared them
	FAnimationRuntime::UpdateDesiredBoneWeight(DesiredBoneBlendWeights, CurrentBoneBlendWeights, BlendWeights);

	TArray<SmartName::UID_Type> const & CurveUIDs = RequiredBones.GetAnimCurveNameUids();
	CurvePoseSourceIndices.Reset(CurveUIDs.Num());
	// initialize with FF - which is default
	CurvePoseSourceIndices.Init(DEFAULT_SOURCEINDEX, CurveUIDs.Num());

	// now go through point to correct source indices. Curve only picks one source index
	for (int32 CurveBlendIter = 0; CurveBlendIter < CurvePoseSourceIndices.Num(); ++CurveBlendIter)
	{
		SmartName::UID_Type CurveUID = CurveUIDs[CurveBlendIter];
		
		const FCurveMetaData* CurveMetaData = Skeleton->GetCurveMetaData(CurveUID);
		if (CurveMetaData && CurveMetaData->LinkedBones.Num() > 0)
		{
			for (int32 LinkedBoneIndex = 0; LinkedBoneIndex < CurveMetaData->LinkedBones.Num(); ++LinkedBoneIndex)
			{
				FCompactPoseBoneIndex CompactPoseIndex = CurveMetaData->LinkedBones[LinkedBoneIndex].GetCompactPoseIndex(RequiredBones);
				if (CompactPoseIndex != INDEX_NONE)
				{
					if (DesiredBoneBlendWeights[CompactPoseIndex.GetInt()].BlendWeight > 0.f)
					{
						CurvePoseSourceIndices[CurveBlendIter] = DesiredBoneBlendWeights[CompactPoseIndex.GetInt()].SourceIndex;
					}
				}
			}
		}
	}
}

void FAnimNode_LayeredBoneBlend::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	BasePose.CacheBones(Context);
	int32 NumPoses = BlendPoses.Num();
	for(int32 ChildIndex=0; ChildIndex<NumPoses; ChildIndex++)
	{
		BlendPoses[ChildIndex].CacheBones(Context);
	}

	if (NumPoses > 0)
	{
		ReinitializeBoneBlendWeights(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
	}
}

void FAnimNode_LayeredBoneBlend::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	bHasRelevantPoses = false;
	int32 RootMotionBlendPose = -1;
	float RootMotionWeight = 0.f;
	const float RootMotionClearWeight = bBlendRootMotionBasedOnRootBone ? 0.f : 1.f;

	for (int32 ChildIndex = 0; ChildIndex < BlendPoses.Num(); ++ChildIndex)
	{
		const float ChildWeight = BlendWeights[ChildIndex];
		if (FAnimWeight::IsRelevant(ChildWeight))
		{
			if (bHasRelevantPoses == false)
			{
				// If our cache is invalid, attempt to update it.
				if (IsCacheInvalid(Context.AnimInstanceProxy->GetSkeleton()))
				{
					ReinitializeBoneBlendWeights(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());

					// If Cache is still invalid, we don't have correct DesiredBoneBlendWeights, so abort.
					// bHasRelevantPoses == false, will passthrough in evaluate.
					if (!ensure(IsCacheInvalid(Context.AnimInstanceProxy->GetSkeleton())))
					{
						break;
					}
				}
				else
				{
					FAnimationRuntime::UpdateDesiredBoneWeight(DesiredBoneBlendWeights, CurrentBoneBlendWeights, BlendWeights);
				}

				bHasRelevantPoses = true;

				if(bBlendRootMotionBasedOnRootBone)
				{
					const float NewRootMotionWeight = CurrentBoneBlendWeights[0].BlendWeight;
					if(NewRootMotionWeight > ZERO_ANIMWEIGHT_THRESH)
					{
						RootMotionWeight = NewRootMotionWeight;
						RootMotionBlendPose = CurrentBoneBlendWeights[0].SourceIndex;
					}
				}
			}

			const float ThisPoseRootMotionWeight = (ChildIndex == RootMotionBlendPose) ? RootMotionWeight : RootMotionClearWeight;
			BlendPoses[ChildIndex].Update(Context.FractionalWeightAndRootMotion(ChildWeight, ThisPoseRootMotionWeight));
		}
	}

	// initialize children
	const float BaseRootMotionWeight = 1.f - RootMotionWeight;

	if (BaseRootMotionWeight < ZERO_ANIMWEIGHT_THRESH)
	{
		BasePose.Update(Context.FractionalWeightAndRootMotion(1.f, BaseRootMotionWeight));
	}
	else
	{
		BasePose.Update(Context);
	}
}

void FAnimNode_LayeredBoneBlend::Evaluate_AnyThread(FPoseContext& Output)
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(BlendPosesInGraph, !IsInGameThread());

	const int NumPoses = BlendPoses.Num();
	if ((NumPoses == 0) || !bHasRelevantPoses)
	{
		BasePose.Evaluate(Output);
	}
	else
	{
		FPoseContext BasePoseContext(Output);

		// evaluate children
		BasePose.Evaluate(BasePoseContext);

		TArray<FCompactPose> TargetBlendPoses;
		TargetBlendPoses.SetNum(NumPoses);

		TArray<FBlendedCurve> TargetBlendCurves;
		TargetBlendCurves.SetNum(NumPoses);

		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			if (FAnimWeight::IsRelevant(BlendWeights[ChildIndex]))
			{
				FPoseContext CurrentPoseContext(Output);
				BlendPoses[ChildIndex].Evaluate(CurrentPoseContext);

				TargetBlendPoses[ChildIndex].CopyBonesFrom(CurrentPoseContext.Pose);
				TargetBlendCurves[ChildIndex].CopyFrom(CurrentPoseContext.Curve);
			}
			else
			{
				TargetBlendPoses[ChildIndex].ResetToRefPose(BasePoseContext.Pose.GetBoneContainer());
				TargetBlendCurves[ChildIndex].InitFrom(Output.Curve);
			}
		}

		// filter to make sure it only includes curves that is linked to the correct bone filter
		const TArray<SmartName::UID_Type>& UIDList = (*Output.Curve.UIDList);
		for (int32 CurveIndex = 0; CurveIndex < CurvePoseSourceIndices.Num(); ++CurveIndex)
		{
			int32 SourceIndex = CurvePoseSourceIndices[CurveIndex];
			if (SourceIndex != DEFAULT_SOURCEINDEX)
			{
				// if source index is set, clear base pose curve value
				BasePoseContext.Curve.Set(UIDList[CurveIndex], 0.f);
				for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
				{
					if (SourceIndex != ChildIndex)
					{
						// if not source, clear it
						TargetBlendCurves[ChildIndex].Set(UIDList[CurveIndex], 0.f);
					}
				}
			}
		}

		FAnimationRuntime::BlendPosesPerBoneFilter(BasePoseContext.Pose, TargetBlendPoses, BasePoseContext.Curve, TargetBlendCurves, Output.Pose, Output.Curve, CurrentBoneBlendWeights, bMeshSpaceRotationBlend, CurveBlendOption);
	}
}


void FAnimNode_LayeredBoneBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	const int NumPoses = BlendPoses.Num();

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Num Poses: %i)"), NumPoses);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData.BranchFlow(1.f));
	
	for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
	{
		BlendPoses[ChildIndex].GatherDebugData(DebugData.BranchFlow(BlendWeights[ChildIndex]));
	}
}

#if WITH_EDITOR
void FAnimNode_LayeredBoneBlend::ValidateData()
{
	// ideally you don't like to get to situation where it becomes inconsistent, but this happened, 
	// and we don't know what caused this. Possibly copy/paste, but I tried copy/paste and that didn't work
	// so here we add code to fix this up manually in editor, so that they can continue working on it. 
	int32 PoseNum = BlendPoses.Num();
	int32 WeightNum = BlendWeights.Num();
	int32 LayerNum = LayerSetup.Num();

	int32 Max = FMath::Max3(PoseNum, WeightNum, LayerNum);
	int32 Min = FMath::Min3(PoseNum, WeightNum, LayerNum);
	// if they are not all same
	if (Min != Max)
	{
		// we'd like to increase to all Max
		// sadly we don't have add X for how many
		for (int32 Index=PoseNum; Index<Max; ++Index)
		{
			BlendPoses.Add(FPoseLink());
		}

		for(int32 Index=WeightNum; Index<Max; ++Index)
		{
			BlendWeights.Add(1.f);
		}

		for(int32 Index=LayerNum; Index<Max; ++Index)
		{
			LayerSetup.Add(FInputBlendPose());
		}
	}
}
#endif
