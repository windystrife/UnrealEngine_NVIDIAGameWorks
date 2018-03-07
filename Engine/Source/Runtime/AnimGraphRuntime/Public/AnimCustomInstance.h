// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

#pragma once
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimCustomInstance.generated.h"

UCLASS(transient, NotBlueprintable)
class ANIMGRAPHRUNTIME_API UAnimCustomInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Called to bind a typed UAnimCustomInstance to an existing skeletal mesh component 
	 * @return the current (or newly created) UAnimCustomInstance
	 */
	template<typename InstanceClassType>
	static InstanceClassType* BindToSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent)
	{
		// make sure to tick and refresh all the time when ticks
		// @TODO: this needs restoring post-binding
		InSkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
#if WITH_EDITOR
		InSkeletalMeshComponent->SetUpdateAnimationInEditor(true);
#endif

		// we use sequence instance if it's using anim blueprint that matches. Otherwise, we create sequence player
		// this might need more check - i.e. making sure if it's same skeleton and so on, 
		// Ideally we could just call NeedToSpawnAnimScriptInstance call, which is protected now
		const bool bShouldUseSequenceInstance = ShouldUseSequenceInstancePlayer(InSkeletalMeshComponent);

		// if it should use sequence instance
		if (bShouldUseSequenceInstance)
		{
			// this has to wrap around with this because we don't want to reinitialize everytime they come here
			// SetAnimationMode will reinitiaize it even if it's same, so make sure we just call SetAnimationMode if not AnimationCustomMode
			if (InSkeletalMeshComponent->GetAnimationMode() != EAnimationMode::AnimationCustomMode)
			{
				InSkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
			}

			if (Cast<UAnimCustomInstance>(InSkeletalMeshComponent->AnimScriptInstance) == nullptr || !InSkeletalMeshComponent->AnimScriptInstance->GetClass()->IsChildOf(InstanceClassType::StaticClass()))
			{
				InstanceClassType* SequencerInstance = NewObject<InstanceClassType>(InSkeletalMeshComponent, InstanceClassType::StaticClass());
				InSkeletalMeshComponent->AnimScriptInstance = SequencerInstance;
				InSkeletalMeshComponent->AnimScriptInstance->InitializeAnimation();
				SequencerInstance->bNeedsUpdate = true;
				return SequencerInstance;
			}
			else
			{
				return Cast<InstanceClassType>(InSkeletalMeshComponent->AnimScriptInstance);
			}
		}

		return nullptr;
	}

	/** Called to unbind a UAnimCustomInstance to an existing skeletal mesh component */
	static void UnbindFromSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent);

private:
	/** Helper function for BindToSkeletalMeshComponent */
	static bool ShouldUseSequenceInstancePlayer(const USkeletalMeshComponent* SkeletalMeshComponent);
};
