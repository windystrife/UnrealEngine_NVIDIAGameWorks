// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_Wait.h"

UBTTask_Wait::UBTTask_Wait(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Wait";
	WaitTime = 5.0f;
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_Wait::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTWaitTaskMemory* MyMemory = (FBTWaitTaskMemory*)NodeMemory;
	MyMemory->RemainingWaitTime = FMath::FRandRange(FMath::Max(0.0f, WaitTime - RandomDeviation), (WaitTime + RandomDeviation));
	
	return EBTNodeResult::InProgress;
}

void UBTTask_Wait::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTWaitTaskMemory* MyMemory = (FBTWaitTaskMemory*)NodeMemory;
	MyMemory->RemainingWaitTime -= DeltaSeconds;

	if (MyMemory->RemainingWaitTime <= 0.0f)
	{
		// continue execution from this node
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

FString UBTTask_Wait::GetStaticDescription() const
{
	if (FMath::IsNearlyZero(RandomDeviation))
	{
		return FString::Printf(TEXT("%s: %.1fs"), *Super::GetStaticDescription(), WaitTime);
	}
	else
	{
		return FString::Printf(TEXT("%s: %.1f+-%.1fs"), *Super::GetStaticDescription(), WaitTime, RandomDeviation);
	}
}

void UBTTask_Wait::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	FBTWaitTaskMemory* MyMemory = (FBTWaitTaskMemory*)NodeMemory;
	if (MyMemory->RemainingWaitTime)
	{
		Values.Add(FString::Printf(TEXT("remaining: %ss"), *FString::SanitizeFloat(MyMemory->RemainingWaitTime)));
	}
}

uint16 UBTTask_Wait::GetInstanceMemorySize() const
{
	return sizeof(FBTWaitTaskMemory);
}

#if WITH_EDITOR

FName UBTTask_Wait::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.Wait.Icon");
}

#endif	// WITH_EDITOR
