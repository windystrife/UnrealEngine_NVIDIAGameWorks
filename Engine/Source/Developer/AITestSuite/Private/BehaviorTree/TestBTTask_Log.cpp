// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/TestBTTask_Log.h"
#include "MockAI_BT.h"

UTestBTTask_Log::UTestBTTask_Log(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Log";
	ExecutionTicks = 0;
	LogIndex = 0;
	LogFinished = -1;
	LogResult = EBTNodeResult::Succeeded;

	bNotifyTick = true;
}

EBTNodeResult::Type UTestBTTask_Log::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTLogTaskMemory* MyMemory = (FBTLogTaskMemory*)NodeMemory;
	MyMemory->EndFrameIdx = ExecutionTicks + FAITestHelpers::FramesCounter();

	LogExecution(OwnerComp, LogIndex);
	if (ExecutionTicks == 0)
	{
		return LogResult;
	}

	return EBTNodeResult::InProgress;
}

void UTestBTTask_Log::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTLogTaskMemory* MyMemory = (FBTLogTaskMemory*)NodeMemory;

	if (FAITestHelpers::FramesCounter() >= MyMemory->EndFrameIdx)
	{
		LogExecution(OwnerComp, LogFinished);
		FinishLatentTask(OwnerComp, LogResult);
	}
}

uint16 UTestBTTask_Log::GetInstanceMemorySize() const
{
	return sizeof(FBTLogTaskMemory);
}

void UTestBTTask_Log::LogExecution(UBehaviorTreeComponent& OwnerComp, int32 LogNumber)
{
	if (LogNumber >= 0)
	{
		UMockAI_BT::ExecutionLog.Add(LogNumber);
	}
}
