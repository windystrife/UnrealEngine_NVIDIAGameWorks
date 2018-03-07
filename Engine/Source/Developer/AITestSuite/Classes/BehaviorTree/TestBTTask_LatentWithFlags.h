// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TestBTTask_LatentWithFlags.generated.h"

struct FBTLatentTaskMemory
{
	uint64 FlagFrameIdx;
	uint64 EndFrameIdx;
	uint8 bFlagSet : 1;
	uint8 bIsAborting : 1;
};

UCLASS(meta = (HiddenNode))
class UTestBTTask_LatentWithFlags : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 LogIndexExecuteStart;

	UPROPERTY()
	int32 LogIndexExecuteFinish;

	UPROPERTY()
	int32 LogIndexAbortStart;

	UPROPERTY()
	int32 LogIndexAbortFinish;

	UPROPERTY()
	int32 ExecuteTicks;

	UPROPERTY()
	int32 AbortTicks;

	UPROPERTY()
	FName KeyNameExecute;

	UPROPERTY()
	FName KeyNameAbort;

	UPROPERTY()
	TEnumAsByte<EBTNodeResult::Type> LogResult;

	virtual EBTNodeResult::Type ExecuteTask(class UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	void LogExecution(class UBehaviorTreeComponent& OwnerComp, int32 LogNumber);

protected:
	virtual void TickTask(class UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
