// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TestBTTask_SetValue.generated.h"

UCLASS(meta=(HiddenNode))
class UTestBTTask_SetValue : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FName KeyName;

	UPROPERTY()
	int32 Value;

	UPROPERTY()
	TEnumAsByte<EBTNodeResult::Type> TaskResult;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
