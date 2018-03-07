// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/TestBTService_Log.h"
#include "MockAI_BT.h"

UTestBTService_Log::UTestBTService_Log(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "LogService";

	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;

	LogActivation = INDEX_NONE;
	LogDeactivation = INDEX_NONE;
}

void UTestBTService_Log::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (LogActivation >= 0)
	{
		UMockAI_BT::ExecutionLog.Add(LogActivation);
	}
}

void UTestBTService_Log::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	if (LogDeactivation >= 0)
	{
		UMockAI_BT::ExecutionLog.Add(LogDeactivation);
	}
}
