// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequencerAnimInstanceProxy.h"
#include "ControlRigSequencerAnimInstance.h"

void FControlRigSequencerAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimSequencerInstanceProxy::Initialize(InAnimInstance);

	// insert our extension nodes just after the root
	FAnimNode_Base* OldBaseLinkedNode = SequencerRootNode.Base.GetLinkNode();
	FAnimNode_Base* OldAdditiveLinkedNode = SequencerRootNode.Additive.GetLinkNode();

	SequencerRootNode.Base.SetLinkNode(&LayeredBoneBlendNode);
	SequencerRootNode.Additive.SetLinkNode(&AdditiveLayeredBoneBlendNode);

	LayeredBoneBlendNode.BasePose.SetLinkNode(OldBaseLinkedNode);
	AdditiveLayeredBoneBlendNode.BasePose.SetLinkNode(OldAdditiveLinkedNode);

	FAnimationInitializeContext Context(this);
	LayeredBoneBlendNode.Initialize_AnyThread(Context);
	AdditiveLayeredBoneBlendNode.Initialize_AnyThread(Context);
}

void FControlRigSequencerAnimInstanceProxy::Update(float DeltaSeconds)
{
	if (bLayeredBlendChanged)
	{
		LayeredBoneBlendNode.ReinitializeBoneBlendWeights(GetRequiredBones(), GetSkeleton());
		bLayeredBlendChanged = false;
	}
	if (bAdditiveLayeredBlendChanged)
	{
		AdditiveLayeredBoneBlendNode.ReinitializeBoneBlendWeights(GetRequiredBones(), GetSkeleton());
		bAdditiveLayeredBlendChanged = false;
	}

	FAnimSequencerInstanceProxy::Update(DeltaSeconds);
}

void FControlRigSequencerAnimInstanceProxy::CacheBones()
{
	// As we dont use the RootNode (this is not using anim blueprint), 
	// we just cache the nodes we know about
	if (bBoneCachesInvalidated)
	{
		bBoneCachesInvalidated = false;

		CachedBonesCounter.Increment();
		FAnimationCacheBonesContext Context(this);
		SequencerRootNode.CacheBones_AnyThread(Context);
	}
}

void FControlRigSequencerAnimInstanceProxy::ResetNodes()
{
	FAnimSequencerInstanceProxy::ResetNodes();

	FMemory::Memzero(LayeredBoneBlendNode.BlendWeights.GetData(), AdditiveLayeredBoneBlendNode.BlendWeights.GetAllocatedSize());
	FMemory::Memzero(LayeredBoneBlendNode.BlendWeights.GetData(), AdditiveLayeredBoneBlendNode.BlendWeights.GetAllocatedSize());
}

void FControlRigSequencerAnimInstanceProxy::InitControlRigTrack(UControlRig* InControlRig, bool bAdditive, bool bApplyBoneFilter, const FInputBlendPose& BoneFilter, uint32 SequenceId)
{
	if (InControlRig != nullptr)
	{
		FSequencerPlayerControlRig* PlayerState = FindValidPlayerState(InControlRig, bAdditive, bApplyBoneFilter, BoneFilter, SequenceId);
		if (PlayerState == nullptr)
		{
			if (bApplyBoneFilter)
			{
				// We are filtering by bone
				FAnimNode_LayeredBoneBlend& LayeredBlendNode = bAdditive ? AdditiveLayeredBoneBlendNode : LayeredBoneBlendNode;

				const int32 PoseIndex = LayeredBlendNode.BlendPoses.Num();
				LayeredBlendNode.AddPose();

				// add the new entry to map
				FSequencerPlayerControlRig* NewPlayerState = new FSequencerPlayerControlRig();
				NewPlayerState->PoseIndex = PoseIndex;

				SequencerToPlayerMap.Add(SequenceId, NewPlayerState);

				// link ControlRig node
				LayeredBlendNode.BlendPoses[PoseIndex].SetLinkNode(&NewPlayerState->ControlRigNode);
				LayeredBlendNode.LayerSetup[PoseIndex] = BoneFilter;
				LayeredBlendNode.BlendWeights[PoseIndex] = 0.0f;

				// Reinit layered blend to rebuild per-bone blend weights next eval
				if (bAdditive)
				{
					bAdditiveLayeredBlendChanged = true;
				}
				else
				{
					bLayeredBlendChanged = true;
				}

				// set player state
				PlayerState = NewPlayerState;
			}
			else
			{
				// Full-body animation
				FAnimNode_MultiWayBlend& BlendNode = bAdditive ? AdditiveBlendNode : FullBodyBlendNode;

				const int32 PoseIndex = BlendNode.AddPose() - 1;

				// add the new entry to map
				FSequencerPlayerControlRig* NewPlayerState = new FSequencerPlayerControlRig();
				NewPlayerState->PoseIndex = PoseIndex;

				SequencerToPlayerMap.Add(SequenceId, NewPlayerState);

				// link ControlRig node
				BlendNode.Poses[PoseIndex].SetLinkNode(&NewPlayerState->ControlRigNode);

				// set player state
				PlayerState = NewPlayerState;
			}
		}

		// now set animation data to player
		PlayerState->ControlRigNode.SetControlRig(InControlRig);
		PlayerState->ControlRigNode.bAdditive = bAdditive;
		PlayerState->bApplyBoneFilter = bApplyBoneFilter;
		PlayerState->bAdditive = bAdditive;

		// initialize player
		PlayerState->ControlRigNode.OnInitializeAnimInstance(this, CastChecked<UAnimInstance>(GetAnimInstanceObject()));
		PlayerState->ControlRigNode.Initialize_AnyThread(FAnimationInitializeContext(this));
	}
}

bool FControlRigSequencerAnimInstanceProxy::UpdateControlRig(UControlRig* InControlRig, uint32 SequenceId, bool bAdditive, bool bApplyBoneFilter, const FInputBlendPose& BoneFilter, float Weight)
{
	bool bCreated = EnsureControlRigTrack(InControlRig, bAdditive, bApplyBoneFilter, BoneFilter, SequenceId);

	FSequencerPlayerControlRig* PlayerState = FindPlayer<FSequencerPlayerControlRig>(SequenceId);
	if (bApplyBoneFilter)
	{
		FAnimNode_LayeredBoneBlend& LayeredBlendNode = bAdditive ? AdditiveLayeredBoneBlendNode : LayeredBoneBlendNode;
		LayeredBlendNode.BlendWeights[PlayerState->PoseIndex] = Weight;
	}
	else
	{
		FAnimNode_MultiWayBlend& BlendNode = bAdditive ? AdditiveBlendNode : FullBodyBlendNode;
		BlendNode.DesiredAlphas[PlayerState->PoseIndex] = Weight;
	}

	return bCreated;
}

bool FControlRigSequencerAnimInstanceProxy::EnsureControlRigTrack(UControlRig* InControlRig, bool bAdditive, bool bApplyBoneFilter, const FInputBlendPose& BoneFilter, uint32 SequenceId)
{
	if (!FindValidPlayerState(InControlRig, bAdditive, bApplyBoneFilter, BoneFilter, SequenceId))
	{
		InitControlRigTrack(InControlRig, bAdditive, bApplyBoneFilter, BoneFilter, SequenceId);
		return true;
	}

	return false;
}

FSequencerPlayerControlRig* FControlRigSequencerAnimInstanceProxy::FindValidPlayerState(UControlRig* InControlRig, bool bAdditive, bool bApplyBoneFilter, const FInputBlendPose& BoneFilter, uint32 SequenceId)
{
	FSequencerPlayerControlRig* PlayerState = FindPlayer<FSequencerPlayerControlRig>(SequenceId);
	if (PlayerState == nullptr)
	{
		return nullptr;
	}
	else if (InControlRig != PlayerState->ControlRigNode.GetControlRig() || bAdditive != PlayerState->bAdditive || bApplyBoneFilter != PlayerState->bApplyBoneFilter)
	{
		// If our criteria are different, force our weight to zero as we will (probably) occupy a new slot this time
		if (PlayerState->bApplyBoneFilter)
		{
			FAnimNode_LayeredBoneBlend& LayeredBlendNode = PlayerState->bAdditive ? AdditiveLayeredBoneBlendNode : LayeredBoneBlendNode;
			LayeredBlendNode.BlendWeights[PlayerState->PoseIndex] = 0.0f;
		}
		else
		{
			FAnimNode_MultiWayBlend& BlendNode = PlayerState->bAdditive ? AdditiveBlendNode : FullBodyBlendNode;
			BlendNode.DesiredAlphas[PlayerState->PoseIndex] = 0.0f;
		}

		return nullptr;
	}
	return PlayerState;
}