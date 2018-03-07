// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_ScaleChainLength.h"
#include "SceneManagement.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_ScaleChainLength

FAnimNode_ScaleChainLength::FAnimNode_ScaleChainLength()
	: Alpha(1.f)
{
	ChainInitialLength = EScaleChainInitialLength::FixedDefaultLengthValue;
}

void FAnimNode_ScaleChainLength::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	InputPose.Initialize(Context);
}

void FAnimNode_ScaleChainLength::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
	InputPose.Update(Context);

	ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
}

void FAnimNode_ScaleChainLength::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	InputPose.CacheBones(Context);

	// LOD change, recache bone indices.
	bBoneIndicesCached = false;
}

void FAnimNode_ScaleChainLength::Evaluate_AnyThread(FPoseContext& Output)
{
	// Evaluate incoming pose into our output buffer.
	InputPose.Evaluate(Output);

	if (!FAnimWeight::IsRelevant(ActualAlpha))
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();

	if (!bBoneIndicesCached)
	{
		bBoneIndicesCached = true;

		ChainStartBone.Initialize(BoneContainer);
		ChainEndBone.Initialize(BoneContainer);
		ChainBoneIndices.Reset();

		// Make sure we have valid start/end bones, and that end is a child of start.
		// Cache this, so we only evaluate on init and LOD changes.
		const bool bBoneSetupIsValid = ChainStartBone.IsValidToEvaluate(BoneContainer) && ChainEndBone.IsValidToEvaluate(BoneContainer) &&
			BoneContainer.BoneIsChildOf(ChainEndBone.GetCompactPoseIndex(BoneContainer), ChainStartBone.GetCompactPoseIndex(BoneContainer));

		if (bBoneSetupIsValid)
		{
			const FCompactPoseBoneIndex StartBoneIndex = ChainStartBone.GetCompactPoseIndex(BoneContainer);
			FCompactPoseBoneIndex BoneIndex = ChainEndBone.GetCompactPoseIndex(BoneContainer);
			ChainBoneIndices.Add(BoneIndex);
			if (BoneIndex != INDEX_NONE)
			{
				FCompactPoseBoneIndex ParentBoneIndex = BoneContainer.GetParentBoneIndex(BoneIndex);
				while ((ParentBoneIndex != INDEX_NONE) && (ParentBoneIndex != StartBoneIndex))
				{
					BoneIndex = ParentBoneIndex;
					ParentBoneIndex = BoneContainer.GetParentBoneIndex(BoneIndex);
					ChainBoneIndices.Insert(BoneIndex, 0);
				};
				ChainBoneIndices.Insert(StartBoneIndex, 0);
			}
		}
	}

	// Need at least Start/End bones to be valid.
	if (ChainBoneIndices.Num() < 2)
	{
		return;
	}

	const FVector TargetLocationCompSpace = Output.AnimInstanceProxy->GetSkelMeshCompLocalToWorld().InverseTransformPosition(TargetLocation);

	// Allocate transforms to get component space transform of chain start bone.
	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);

	const FTransform StartTransformCompSpace = CSPose.GetComponentSpaceTransform(ChainBoneIndices[0]);

	const float DesiredChainLength = (TargetLocationCompSpace - StartTransformCompSpace.GetLocation()).Size();
	const float InitialChainLength = GetInitialChainLength(Output.Pose, CSPose);
	const float ChainLengthScale = !FMath::IsNearlyZero(InitialChainLength) ? (DesiredChainLength / InitialChainLength) : 1.f;
	const float ChainLengthScaleWithAlpha = FMath::LerpStable(1.f, ChainLengthScale, ActualAlpha);

	// If we're not going to scale anything, early out.
	if (FMath::IsNearlyEqual(ChainLengthScaleWithAlpha, 1.f))
	{
		return;
	}

	// Scale translation of all bones in local space.
	FCompactPose& LSPose = Output.Pose;
	for (const FCompactPoseBoneIndex& BoneIndex : ChainBoneIndices)
	{
		// Get bone space transform, scale transition.
		LSPose[BoneIndex].ScaleTranslation(ChainLengthScaleWithAlpha);
	}
}

float FAnimNode_ScaleChainLength::GetInitialChainLength(FCompactPose& InLSPose, FCSPose<FCompactPose>& InCSPose) const
{
	switch (ChainInitialLength)
	{
	case EScaleChainInitialLength::Distance : 
	{
		const FVector ChainStartLocation = InCSPose.GetComponentSpaceTransform(ChainBoneIndices[0]).GetLocation();
		const FVector ChainEndLocation = InCSPose.GetComponentSpaceTransform(ChainBoneIndices.Last()).GetLocation();
		return (ChainEndLocation - ChainStartLocation).Size();
	}

	case EScaleChainInitialLength::ChainLength :
	{
		float ChainLength = 0.f;
		for (const FCompactPoseBoneIndex& BoneIndex : ChainBoneIndices)
		{
			ChainLength += InLSPose[BoneIndex].GetTranslation().Size();
		}
		return ChainLength;
	}
	};

	// Fallback is using fixed value DefaultChainLength.
	return DefaultChainLength;
}

void FAnimNode_ScaleChainLength::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("Alpha (%.1f%%)"), ActualAlpha * 100.f);
	DebugData.AddDebugItem(DebugLine);

	InputPose.GatherDebugData(DebugData);
}
