// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PlayAnimation.generated.h"

/**
 *	Play indicated AnimationAsset on Pawn controlled by BT 
 *	Note that this node is generic and is handing multiple special cases,
 *	If you want a more efficient solution you'll need to implement it yourself (or wait for our BTTask_PlayCharacterAnimation)
 */
UCLASS()
class AIMODULE_API UBTTask_PlayAnimation : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	/** Animation asset to play. Note that it needs to match the skeleton of pawn this BT is controlling */
	UPROPERTY(Category = Node, EditAnywhere)
	UAnimationAsset* AnimationToPlay;
	
	UPROPERTY(Category = Node, EditAnywhere)
	uint32 bLooping : 1; 

	/** if true the task will just trigger the animation and instantly finish. Fire and Forget. */
	UPROPERTY(Category = Node, EditAnywhere)
	uint32 bNonBlocking : 1;

	UPROPERTY()
	UBehaviorTreeComponent* MyOwnerComp;

	UPROPERTY()
	USkeletalMeshComponent* CachedSkelMesh;

	EAnimationMode::Type PreviousAnimationMode;

	FTimerDelegate TimerDelegate;
	FTimerHandle TimerHandle;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	void OnAnimationTimerDone();
	
#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:
	void CleanUp(UBehaviorTreeComponent& OwnerComp);
};
