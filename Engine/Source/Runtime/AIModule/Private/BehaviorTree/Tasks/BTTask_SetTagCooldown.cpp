// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_SetTagCooldown.h"

UBTTask_SetTagCooldown::UBTTask_SetTagCooldown(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Set Tag Cooldown";
	CooldownDuration = 5.0f;	
	bAddToExistingDuration = false;
}

EBTNodeResult::Type UBTTask_SetTagCooldown::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	OwnerComp.AddCooldownTagDuration(CooldownTag, CooldownDuration, bAddToExistingDuration);
	
	return EBTNodeResult::Succeeded;
}

FString UBTTask_SetTagCooldown::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s %s: %.1fs"), *Super::GetStaticDescription(), *CooldownTag.ToString(), CooldownDuration);
}

#if WITH_EDITOR

FName UBTTask_SetTagCooldown::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Cooldown.Icon");
}

#endif	// WITH_EDITOR
