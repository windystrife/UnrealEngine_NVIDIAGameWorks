// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequencerAnimInstance.h"
#include "ControlRigSequencerAnimInstanceProxy.h"

UControlRigSequencerAnimInstance::UControlRigSequencerAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = false;
}

FAnimInstanceProxy* UControlRigSequencerAnimInstance::CreateAnimInstanceProxy()
{
	return new FControlRigSequencerAnimInstanceProxy(this);
}

bool UControlRigSequencerAnimInstance::UpdateControlRig(UControlRig* InControlRig, uint32 SequenceId, bool bAdditive, bool bApplyBoneFilter, const FInputBlendPose& BoneFilter, float Weight)
{
	return GetProxyOnGameThread<FControlRigSequencerAnimInstanceProxy>().UpdateControlRig(InControlRig, SequenceId, bAdditive, bApplyBoneFilter, BoneFilter, Weight);
}