// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_BlendBoneByChannel.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendBoneByChannel

void FAnimNode_BlendBoneByChannel::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	A.Initialize(Context);
	B.Initialize(Context);
}

void FAnimNode_BlendBoneByChannel::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	A.CacheBones(Context);
	B.CacheBones(Context);

	// Pre-validate bone entries, so we don't waste cycles every frame figuring it out.
	ValidBoneEntries.Reset();
	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
	for (FBlendBoneByChannelEntry& Entry : BoneDefinitions)
	{
		Entry.SourceBone.Initialize(BoneContainer);
		Entry.TargetBone.Initialize(BoneContainer);

		if (Entry.SourceBone.IsValidToEvaluate(BoneContainer) && Entry.TargetBone.IsValidToEvaluate(BoneContainer)
			&& (Entry.bBlendTranslation || Entry.bBlendRotation || Entry.bBlendScale))
		{
			ValidBoneEntries.Add(Entry);
		}
	}
}

void FAnimNode_BlendBoneByChannel::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnimNode_BlendBoneByChannel_Update);
	EvaluateGraphExposedInputs.Execute(Context);

	InternalBlendAlpha = AlphaScaleBias.ApplyTo(Alpha);
	bBIsRelevant = FAnimWeight::IsRelevant(InternalBlendAlpha) && (ValidBoneEntries.Num() > 0);

	A.Update(Context);
	if (bBIsRelevant)
	{
		B.Update(Context.FractionalWeight(InternalBlendAlpha));
	}
}

void FAnimNode_BlendBoneByChannel::Evaluate_AnyThread(FPoseContext& Output)
{
	A.Evaluate(Output);

	if (bBIsRelevant)
	{
		const FBoneContainer& BoneContainer = Output.AnimInstanceProxy->GetRequiredBones();

		FPoseContext PoseB(Output);
		B.Evaluate(PoseB);

		// Faster code path in local space
		if (TransformsSpace == EBoneControlSpace::BCS_BoneSpace)
		{
			const FCompactPose& SourcePose = PoseB.Pose;
			FCompactPose& TargetPose = Output.Pose;

			for (const FBlendBoneByChannelEntry& Entry : ValidBoneEntries)
			{
				const FCompactPoseBoneIndex SourceBoneIndex = Entry.SourceBone.GetCompactPoseIndex(BoneContainer);
				const FCompactPoseBoneIndex TargetBoneIndex = Entry.TargetBone.GetCompactPoseIndex(BoneContainer);

				const FTransform SourceTransform = SourcePose[SourceBoneIndex];
				FTransform& TargetTransform = TargetPose[TargetBoneIndex];

				// Blend Transforms.
				FTransform BlendedTransform;
				BlendedTransform.Blend(TargetTransform, SourceTransform, InternalBlendAlpha);

				// Filter through channels
				{
					if (Entry.bBlendTranslation)
					{
						TargetTransform.SetTranslation(BlendedTransform.GetTranslation());
					}

					if (Entry.bBlendRotation)
					{
						TargetTransform.SetRotation(BlendedTransform.GetRotation());
					}

					if (Entry.bBlendScale)
					{
						TargetTransform.SetScale3D(BlendedTransform.GetScale3D());
					}
				}
			}
		}
		// Slower code path where local transforms have to be converted to a different space.
		else
		{
			FCSPose<FCompactPose> TargetPoseCmpntSpace;
			TargetPoseCmpntSpace.InitPose(Output.Pose);

			FCSPose<FCompactPose> SourcePoseCmpntSpace;
			SourcePoseCmpntSpace.InitPose(PoseB.Pose);

			TArray<FBoneTransform> QueuedModifiedBoneTransforms;

			const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

			for (const FBlendBoneByChannelEntry& Entry : ValidBoneEntries)
			{
				const FCompactPoseBoneIndex SourceBoneIndex = Entry.SourceBone.GetCompactPoseIndex(BoneContainer);
				const FCompactPoseBoneIndex TargetBoneIndex = Entry.TargetBone.GetCompactPoseIndex(BoneContainer);

				FTransform SourceTransform = SourcePoseCmpntSpace.GetComponentSpaceTransform(SourceBoneIndex);
				FTransform TargetTransform = TargetPoseCmpntSpace.GetComponentSpaceTransform(TargetBoneIndex);

				// Convert Transforms to correct space.
				FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, SourcePoseCmpntSpace, SourceTransform, SourceBoneIndex, TransformsSpace);
				FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, TargetPoseCmpntSpace, TargetTransform, TargetBoneIndex, TransformsSpace);

				// Blend Transforms.
				FTransform BlendedTransform;
				BlendedTransform.Blend(TargetTransform, SourceTransform, InternalBlendAlpha);

				// Filter through channels
				{
					if (Entry.bBlendTranslation)
					{
						TargetTransform.SetTranslation(BlendedTransform.GetTranslation());
					}

					if (Entry.bBlendRotation)
					{
						TargetTransform.SetRotation(BlendedTransform.GetRotation());
					}

					if (Entry.bBlendScale)
					{
						TargetTransform.SetScale3D(BlendedTransform.GetScale3D());
					}
				}

				// Convert blended and filtered result back in component space.
				FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, TargetPoseCmpntSpace, TargetTransform, TargetBoneIndex, TransformsSpace);

				// Queue transform to be applied after all transforms have been created.
				// So we don't have parent bones affecting children bones.
				QueuedModifiedBoneTransforms.Add(FBoneTransform(TargetBoneIndex, TargetTransform));
			}

			if (QueuedModifiedBoneTransforms.Num() > 0)
			{
				// Sort OutBoneTransforms so indices are in increasing order.
				QueuedModifiedBoneTransforms.Sort(FCompareBoneTransformIndex());

				// Apply transforms
				TargetPoseCmpntSpace.SafeSetCSBoneTransforms(QueuedModifiedBoneTransforms);

				// Turn Component Space poses back into local space.
				TargetPoseCmpntSpace.ConvertToLocalPoses(Output.Pose);
			}
		}
	}
}

void FAnimNode_BlendBoneByChannel::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%)"), InternalBlendAlpha * 100);
	DebugData.AddDebugItem(DebugLine);

	A.GatherDebugData(DebugData.BranchFlow(1.f));
	B.GatherDebugData(DebugData.BranchFlow(InternalBlendAlpha));
}
