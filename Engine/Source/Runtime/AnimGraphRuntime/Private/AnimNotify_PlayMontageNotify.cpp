// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_PlayMontageNotify.h"
#include "Animation/AnimInstance.h"

//////////////////////////////////////////////////////////////////////////
// UAnimNotify_PlayMontageNotify
//////////////////////////////////////////////////////////////////////////

UAnimNotify_PlayMontageNotify::UAnimNotify_PlayMontageNotify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsNativeBranchingPoint = true;
}


void UAnimNotify_PlayMontageNotify::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotify(BranchingPointPayload);

	if (USkeletalMeshComponent* MeshComp = BranchingPointPayload.SkelMeshComponent)
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->OnPlayMontageNotifyBegin.Broadcast(NotifyName, BranchingPointPayload);
		}
	}
}

#if WITH_EDITOR
bool UAnimNotify_PlayMontageNotify::CanBePlaced(UAnimSequenceBase* Animation) const
{
	return (Animation && Animation->IsA(UAnimMontage::StaticClass()));
}
#endif // WITH_EDITOR


//////////////////////////////////////////////////////////////////////////
// UAnimNotify_PlayMontageNotifyWindow
//////////////////////////////////////////////////////////////////////////

UAnimNotify_PlayMontageNotifyWindow::UAnimNotify_PlayMontageNotifyWindow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsNativeBranchingPoint = true;
}


void UAnimNotify_PlayMontageNotifyWindow::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyBegin(BranchingPointPayload);

	if (USkeletalMeshComponent* MeshComp = BranchingPointPayload.SkelMeshComponent)
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->OnPlayMontageNotifyBegin.Broadcast(NotifyName, BranchingPointPayload);
		}
	}
}


void UAnimNotify_PlayMontageNotifyWindow::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyEnd(BranchingPointPayload);

	if (USkeletalMeshComponent* MeshComp = BranchingPointPayload.SkelMeshComponent)
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->OnPlayMontageNotifyEnd.Broadcast(NotifyName, BranchingPointPayload);
		}
	}
}

#if WITH_EDITOR
bool UAnimNotify_PlayMontageNotifyWindow::CanBePlaced(UAnimSequenceBase* Animation) const
{
	return (Animation && Animation->IsA(UAnimMontage::StaticClass()));
}
#endif // WITH_EDITOR