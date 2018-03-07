// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PlaySound.generated.h"

class USoundCue;

/**
 * Play Sound task node.
 * Plays the specified sound when executed.
 */
UCLASS()
class AIMODULE_API UBTTask_PlaySound : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	/** CUE to play */
	UPROPERTY(Category=Node, EditAnywhere)
	USoundCue* SoundToPlay;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
