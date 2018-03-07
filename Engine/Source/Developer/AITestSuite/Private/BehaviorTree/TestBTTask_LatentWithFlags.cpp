// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/TestBTTask_LatentWithFlags.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MockAI_BT.h"

UTestBTTask_LatentWithFlags::UTestBTTask_LatentWithFlags(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "LatentTest";
	LogIndexExecuteStart = 0;
	LogIndexExecuteFinish = 0;
	LogIndexAbortStart = 0;
	LogIndexAbortFinish = 0;
	ExecuteTicks = 2;
	AbortTicks = 2;
	KeyNameExecute = TEXT("Bool1");
	KeyNameAbort = TEXT("Bool2");
	LogResult = EBTNodeResult::Succeeded;

	bNotifyTick = true;
}

EBTNodeResult::Type UTestBTTask_LatentWithFlags::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTLatentTaskMemory* MyMemory = (FBTLatentTaskMemory*)NodeMemory;
	MyMemory->FlagFrameIdx = ExecuteTicks + FAITestHelpers::FramesCounter();
	MyMemory->EndFrameIdx = MyMemory->FlagFrameIdx + ExecuteTicks;
	MyMemory->bFlagSet = false;
	MyMemory->bIsAborting = false;

	LogExecution(OwnerComp, LogIndexExecuteStart);
	if (ExecuteTicks == 0)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(KeyNameExecute, true);
		MyMemory->bFlagSet = true;

		LogExecution(OwnerComp, LogIndexExecuteFinish);
		return LogResult;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UTestBTTask_LatentWithFlags::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTLatentTaskMemory* MyMemory = (FBTLatentTaskMemory*)NodeMemory;
	MyMemory->FlagFrameIdx = AbortTicks + FAITestHelpers::FramesCounter();
	MyMemory->EndFrameIdx = MyMemory->FlagFrameIdx + AbortTicks;
	MyMemory->bFlagSet = false;
	MyMemory->bIsAborting = true;

	LogExecution(OwnerComp, LogIndexAbortStart);
	if (AbortTicks == 0)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(KeyNameAbort, true);
		MyMemory->bFlagSet = true;

		LogExecution(OwnerComp, LogIndexAbortFinish);
		return EBTNodeResult::Aborted;
	}

	return EBTNodeResult::InProgress;
}

void UTestBTTask_LatentWithFlags::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTLatentTaskMemory* MyMemory = (FBTLatentTaskMemory*)NodeMemory;

	if (!MyMemory->bFlagSet && FAITestHelpers::FramesCounter() >= MyMemory->FlagFrameIdx)
	{
		MyMemory->bFlagSet = true;
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(
			MyMemory->bIsAborting ? KeyNameAbort : KeyNameExecute,
			true);
	}

	if (FAITestHelpers::FramesCounter() >= MyMemory->EndFrameIdx)
	{
		if (MyMemory->bIsAborting)
		{
			LogExecution(OwnerComp, LogIndexAbortFinish);
			FinishLatentAbort(OwnerComp);
		}
		else
		{
			LogExecution(OwnerComp, LogIndexExecuteFinish);
			FinishLatentTask(OwnerComp, LogResult);
		}
	}	
}

uint16 UTestBTTask_LatentWithFlags::GetInstanceMemorySize() const
{
	return sizeof(FBTLatentTaskMemory);
}

void UTestBTTask_LatentWithFlags::LogExecution(UBehaviorTreeComponent& OwnerComp, int32 LogNumber)
{
	if (LogNumber >= 0)
	{
		UMockAI_BT::ExecutionLog.Add(LogNumber);
	}
}
