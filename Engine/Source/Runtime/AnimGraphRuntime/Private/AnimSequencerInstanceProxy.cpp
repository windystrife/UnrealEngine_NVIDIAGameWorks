// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimSequencerInstanceProxy.h"
#include "AnimSequencerInstance.h"

void FAnimSequencerInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	ConstructNodes();

	UpdateCounter.Reset();
}

bool FAnimSequencerInstanceProxy::Evaluate(FPoseContext& Output)
{
	SequencerRootNode.Evaluate_AnyThread(Output);

	return true;
}

void FAnimSequencerInstanceProxy::UpdateAnimationNode(float DeltaSeconds)
{
	UpdateCounter.Increment();

	SequencerRootNode.Update_AnyThread(FAnimationUpdateContext(this, DeltaSeconds));
}

void FAnimSequencerInstanceProxy::ConstructNodes()
{
	// construct node link node for full body and additive to apply additive node
	SequencerRootNode.Base.SetLinkNode(&FullBodyBlendNode);
	SequencerRootNode.Additive.SetLinkNode(&AdditiveBlendNode);

	FullBodyBlendNode.bAdditiveNode = false;
	FullBodyBlendNode.bNormalizeAlpha = true;

	AdditiveBlendNode.bAdditiveNode = true;
	AdditiveBlendNode.bNormalizeAlpha = false;

	FullBodyBlendNode.ResetPoses();
	AdditiveBlendNode.ResetPoses();

	ClearSequencePlayerMap();
}

void FAnimSequencerInstanceProxy::ClearSequencePlayerMap()
{
	for (TPair<uint32, FSequencerPlayerBase*>& Iter : SequencerToPlayerMap)
	{
		delete Iter.Value;
	}

	SequencerToPlayerMap.Empty();
}

void FAnimSequencerInstanceProxy::ResetNodes()
{
	FMemory::Memzero(FullBodyBlendNode.DesiredAlphas.GetData(), FullBodyBlendNode.DesiredAlphas.GetAllocatedSize());
	FMemory::Memzero(AdditiveBlendNode.DesiredAlphas.GetData(), AdditiveBlendNode.DesiredAlphas.GetAllocatedSize());
}

FAnimSequencerInstanceProxy::~FAnimSequencerInstanceProxy()
{
	ClearSequencePlayerMap();
}

void FAnimSequencerInstanceProxy::InitAnimTrack(UAnimSequenceBase* InAnimSequence, uint32 SequenceId)
{
	if (InAnimSequence != nullptr)
	{
		FSequencerPlayerAnimSequence* PlayerState = FindPlayer<FSequencerPlayerAnimSequence>(SequenceId);
		if (PlayerState == nullptr)
		{
			const bool bIsAdditive = InAnimSequence->IsValidAdditive();
			FAnimNode_MultiWayBlend& BlendNode = (bIsAdditive) ? AdditiveBlendNode : FullBodyBlendNode;
			
			// you shouldn't allow additive animation to be added here, but if it changes type after
			// you'll see this warning coming up
			if (bIsAdditive && InAnimSequence->GetAdditiveAnimType() == AAT_RotationOffsetMeshSpace)
			{
				// this doesn't work
				UE_LOG(LogAnimation, Warning, TEXT("ERROR: Animation [%s] in Sequencer has Mesh Space additive animation.  No support on mesh space additive animation. "), *GetNameSafe(InAnimSequence));
			}

			const int32 PoseIndex = BlendNode.AddPose() - 1;
			BlendNode.UpdateCachedAlphas();

			// add the new entry to map
			FSequencerPlayerAnimSequence* NewPlayerState = new FSequencerPlayerAnimSequence();
			NewPlayerState->PoseIndex = PoseIndex;
			NewPlayerState->bAdditive = bIsAdditive;
			
			SequencerToPlayerMap.Add(SequenceId, NewPlayerState);

			// link player to blendnode, this will let you trigger notifies and so on
			NewPlayerState->PlayerNode.bTeleportToExplicitTime = false;
			NewPlayerState->PlayerNode.bShouldLoop = true;
			BlendNode.Poses[PoseIndex].SetLinkNode(&NewPlayerState->PlayerNode);

			// set player state
			PlayerState = NewPlayerState;
		}

		// now set animation data to player
		PlayerState->PlayerNode.Sequence = InAnimSequence;
		PlayerState->PlayerNode.ExplicitTime = 0.f;

		// initialize player
		PlayerState->PlayerNode.Initialize_AnyThread(FAnimationInitializeContext(this));
	}
}

/*
// this isn't used yet. If we want to optimize it, we could do this way, but right now the way sequencer updates, we don't have a good point 
// where we could just clear one sequence id. We just clear all the weights before update. 
// once they go out of range, they don't get called anymore, so there is no good point of tearing down
// there is multiple tear down point but we couldn't find where only happens once activated and once getting out
// because sequencer finds the nearest point, not exact point, it doens't have good point of tearing down
void FAnimSequencerInstanceProxy::TermAnimTrack(int32 SequenceId)
{
	FSequencerPlayerState* PlayerState = FindPlayer(SequenceId);

	if (PlayerState)
	{
		FAnimNode_MultiWayBlend& BlendNode = (PlayerState->bAdditive) ? AdditiveBlendNode : FullBodyBlendNode;

		// remove the pose from blend node
		BlendNode.Poses.RemoveAt(PlayerState->PoseIndex);
		BlendNode.DesiredAlphas.RemoveAt(PlayerState->PoseIndex);

		// remove from Sequence Map
		SequencerToPlayerMap.Remove(SequenceId);
	}
}*/

void FAnimSequencerInstanceProxy::UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, uint32 SequenceId, float InPosition, float Weight, bool bFireNotifies)
{
	EnsureAnimTrack(InAnimSequence, SequenceId);

	FSequencerPlayerAnimSequence* PlayerState = FindPlayer<FSequencerPlayerAnimSequence>(SequenceId);
	PlayerState->PlayerNode.ExplicitTime = InPosition;
	// if moving to 0.f, we mark this to teleport. Otherwise, do not use explicit time
	FAnimNode_MultiWayBlend& BlendNode = (PlayerState->bAdditive) ? AdditiveBlendNode : FullBodyBlendNode;
	BlendNode.DesiredAlphas[PlayerState->PoseIndex] = Weight;
}

void FAnimSequencerInstanceProxy::EnsureAnimTrack(UAnimSequenceBase* InAnimSequence, uint32 SequenceId)
{
	FSequencerPlayerAnimSequence* PlayerState = FindPlayer<FSequencerPlayerAnimSequence>(SequenceId);
	if (!PlayerState)
	{
		InitAnimTrack(InAnimSequence, SequenceId);
	}
}