// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UAnimCustomInstance.cpp: Single Node Tree Instance 
	Only plays one animation at a time. 
=============================================================================*/ 

#include "AnimCustomInstance.h"

/////////////////////////////////////////////////////
// UAnimCustomInstance
/////////////////////////////////////////////////////

UAnimCustomInstance::UAnimCustomInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimCustomInstance::UnbindFromSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent)
{
#if WITH_EDITOR
	InSkeletalMeshComponent->SetUpdateAnimationInEditor(false);
#endif

	if (InSkeletalMeshComponent->GetAnimationMode() == EAnimationMode::Type::AnimationCustomMode)
	{
		UAnimCustomInstance* SequencerInstance = Cast<UAnimCustomInstance>(InSkeletalMeshComponent->GetAnimInstance());
		if (SequencerInstance)
		{
			InSkeletalMeshComponent->AnimScriptInstance = nullptr;
		}
	}
	else if (InSkeletalMeshComponent->GetAnimationMode() == EAnimationMode::Type::AnimationBlueprint)
	{
		UAnimInstance* AnimInstance = InSkeletalMeshComponent->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Stop(0.0f);
			AnimInstance->UpdateAnimation(0.0f, false);
		}

		// Update space bases to reset it back to ref pose
		InSkeletalMeshComponent->RefreshBoneTransforms();
		InSkeletalMeshComponent->RefreshSlaveComponents();
		InSkeletalMeshComponent->UpdateComponentToWorld();
	}
}

bool UAnimCustomInstance::ShouldUseSequenceInstancePlayer(const USkeletalMeshComponent* SkeletalMeshComponent)
{
	const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
	// create proper anim instance to animate
	UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();

	return (AnimInstance == nullptr || SkeletalMeshComponent->GetAnimationMode() != EAnimationMode::AnimationBlueprint ||
		AnimInstance->GetClass() != SkeletalMeshComponent->AnimClass || !SkeletalMesh->Skeleton->IsCompatible(AnimInstance->CurrentSkeleton));
}