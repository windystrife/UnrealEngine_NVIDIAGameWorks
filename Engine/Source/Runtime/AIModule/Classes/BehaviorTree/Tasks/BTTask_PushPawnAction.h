// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Tasks/BTTask_PawnActionBase.h"
#include "BTTask_PushPawnAction.generated.h"

class UPawnAction;

/**
 * Action task node.
 * Push pawn action to controller.
 */
UCLASS()
class AIMODULE_API UBTTask_PushPawnAction : public UBTTask_PawnActionBase
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(EditAnywhere, Instanced, Category = Action)
	UPawnAction* Action;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
