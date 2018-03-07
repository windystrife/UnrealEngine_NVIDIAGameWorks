// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UAnimSequencerInstance.cpp: Single Node Tree Instance 
	Only plays one animation at a time. 
=============================================================================*/ 

#include "AnimSequencerInstance.h"
#include "AnimSequencerInstanceProxy.h"

/////////////////////////////////////////////////////
// UAnimSequencerInstance
/////////////////////////////////////////////////////

UAnimSequencerInstance::UAnimSequencerInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = false;
}

FAnimInstanceProxy* UAnimSequencerInstance::CreateAnimInstanceProxy()
{
	return new FAnimSequencerInstanceProxy(this);
}

void UAnimSequencerInstance::UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InPosition, float Weight, bool bFireNotifies)
{
	GetProxyOnGameThread<FAnimSequencerInstanceProxy>().UpdateAnimTrack(InAnimSequence, SequenceId, InPosition, Weight, bFireNotifies);
}

void UAnimSequencerInstance::ResetNodes()
{
	GetProxyOnGameThread<FAnimSequencerInstanceProxy>().ResetNodes();
}