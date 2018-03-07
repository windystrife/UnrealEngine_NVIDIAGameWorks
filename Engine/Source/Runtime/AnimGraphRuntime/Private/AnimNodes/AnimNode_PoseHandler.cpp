// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseHandler.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimPoseByNameNode

void FAnimNode_PoseHandler::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);

	UpdatePoseAssetProperty(Context.AnimInstanceProxy);
}

void FAnimNode_PoseHandler::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	FAnimNode_AssetPlayerBase::CacheBones_AnyThread(Context);

	BoneBlendWeights.Reset();
	// this has to update bone blending weight
	if (CurrentPoseAsset.IsValid())
	{
		const TArray<FName> TrackNames = CurrentPoseAsset.Get()->GetTrackNames();
		const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
		const TArray<FBoneIndexType>& RequiredBoneIndices = BoneContainer.GetBoneIndicesArray();
		BoneBlendWeights.AddZeroed(RequiredBoneIndices.Num());

		for (const auto& TrackName : TrackNames)
		{
			int32 MeshBoneIndex = BoneContainer.GetPoseBoneIndexForBoneName(TrackName);
			FCompactPoseBoneIndex CompactBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBoneIndex));
			if (CompactBoneIndex != INDEX_NONE)
			{
				BoneBlendWeights[CompactBoneIndex.GetInt()] = 1.f;
			}
		}
	}
}

void FAnimNode_PoseHandler::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	// update pose asset if it's not valid
	if (CurrentPoseAsset.IsValid() == false || CurrentPoseAsset.Get() != PoseAsset)
	{
		UpdatePoseAssetProperty(Context.AnimInstanceProxy);
	}
}

void FAnimNode_PoseHandler::OverrideAsset(UAnimationAsset* NewAsset)
{
	if(UPoseAsset* NewPoseAsset = Cast<UPoseAsset>(NewAsset))
	{
		PoseAsset = NewPoseAsset;
	}
}

void FAnimNode_PoseHandler::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s')"), *GetNameSafe(PoseAsset));
	DebugData.AddDebugItem(DebugLine, true);
}

void FAnimNode_PoseHandler::UpdatePoseAssetProperty(struct FAnimInstanceProxy* InstanceProxy)
{
	CurrentPoseAsset = PoseAsset;

	if (CurrentPoseAsset.IsValid())
	{
		const TArray<FSmartName>& PoseNames = CurrentPoseAsset->GetPoseNames();
		const int32 TotalPoseNum = PoseNames.Num();
		if (TotalPoseNum > 0)
		{
			PoseExtractContext.PoseCurves.Reset(TotalPoseNum);
			PoseUIDList.Reset(TotalPoseNum);

			if (TotalPoseNum > 0)
			{
				PoseExtractContext.PoseCurves.AddZeroed(TotalPoseNum);
				for (const auto& PoseName : PoseNames)
				{
					PoseUIDList.Add(PoseName.UID);
				}
			}

			CacheBones_AnyThread(FAnimationCacheBonesContext(InstanceProxy));
		}
		else
		{
			PoseUIDList.Reset();
			PoseExtractContext.PoseCurves.Reset();
			BoneBlendWeights.Reset();
		}
	}
	else
	{
		PoseUIDList.Reset();
		PoseExtractContext.PoseCurves.Reset();
		BoneBlendWeights.Reset();
	}
}
