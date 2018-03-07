// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "AnimPreviewAttacheInstance.h"

#define LOCTEXT_NAMESPACE "AnimPreviewAttacheInstance"

void FAnimPreviewAttacheInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	FAnimationInitializeContext InitContext(this);
	CopyPoseFromMesh.bUseAttachedParent = true;
	CopyPoseFromMesh.Initialize_AnyThread(InitContext);
}

void FAnimPreviewAttacheInstanceProxy::Update(float DeltaSeconds)
{
	// we cant update on a worker thread here because of the key delegate needing to be fired
	check(IsInGameThread());

	FAnimationUpdateContext UpdateContext(this, DeltaSeconds);
	CopyPoseFromMesh.Update_AnyThread(UpdateContext);

	FAnimInstanceProxy::Update(DeltaSeconds);
}

bool FAnimPreviewAttacheInstanceProxy::Evaluate(FPoseContext& Output)
{
	// we cant evaluate on a worker thread here because of the key delegate needing to be fired
	CopyPoseFromMesh.Evaluate_AnyThread(Output);
	return true;
}

UAnimPreviewAttacheInstance::UAnimPreviewAttacheInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootMotionMode = ERootMotionMode::RootMotionFromEverything;
	bUseMultiThreadedAnimationUpdate = false;
}

FAnimInstanceProxy* UAnimPreviewAttacheInstance::CreateAnimInstanceProxy()
{
	return new FAnimPreviewAttacheInstanceProxy(this);
}

#undef LOCTEXT_NAMESPACE
