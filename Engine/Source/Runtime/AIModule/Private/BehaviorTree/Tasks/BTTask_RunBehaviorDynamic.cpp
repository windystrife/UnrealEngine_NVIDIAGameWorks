// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_RunBehaviorDynamic.h"
#include "BehaviorTree/BehaviorTree.h"

UBTTask_RunBehaviorDynamic::UBTTask_RunBehaviorDynamic(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Run Behavior Dynamic";
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_RunBehaviorDynamic::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const bool bPushed = BehaviorAsset != nullptr && OwnerComp.PushInstance(*BehaviorAsset);
	return bPushed ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_RunBehaviorDynamic::OnInstanceCreated(UBehaviorTreeComponent& OwnerComp)
{
	Super::OnInstanceCreated(OwnerComp);
	BehaviorAsset = DefaultBehaviorAsset;
}

FString UBTTask_RunBehaviorDynamic::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *InjectionTag.ToString());
}

void UBTTask_RunBehaviorDynamic::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);
	Values.Add(FString::Printf(TEXT("subtree: %s"), *GetNameSafe(BehaviorAsset)));
}

bool UBTTask_RunBehaviorDynamic::SetBehaviorAsset(UBehaviorTree* NewBehaviorAsset)
{
	if (BehaviorAsset != NewBehaviorAsset)
	{
		BehaviorAsset = NewBehaviorAsset;
		return true;
	}
	return false;
}

#if WITH_EDITOR

FName UBTTask_RunBehaviorDynamic::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.RunBehavior.Icon");
}

#endif	// WITH_EDITOR
