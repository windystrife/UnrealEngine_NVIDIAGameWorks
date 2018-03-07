// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "Engine/World.h"

UBTDecorator_Cooldown::UBTDecorator_Cooldown(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Cooldown";
	CoolDownTime = 5.0f;
	
	// aborting child nodes doesn't makes sense, cooldown starts after leaving this branch
	bAllowAbortChildNodes = false;

	bNotifyTick = false;
	bNotifyDeactivation = true;
}

void UBTDecorator_Cooldown::PostLoad()
{
	Super::PostLoad();
	bNotifyTick = (FlowAbortMode != EBTFlowAbortMode::None);
}

bool UBTDecorator_Cooldown::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	FBTCooldownDecoratorMemory* DecoratorMemory = reinterpret_cast<FBTCooldownDecoratorMemory*>(NodeMemory);
	const float TimePassed = (OwnerComp.GetWorld()->GetTimeSeconds() - DecoratorMemory->LastUseTimestamp);
	return TimePassed >= CoolDownTime;
}

void UBTDecorator_Cooldown::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	FBTCooldownDecoratorMemory* DecoratorMemory = reinterpret_cast<FBTCooldownDecoratorMemory*>(NodeMemory);
	if (InitType == EBTMemoryInit::Initialize)
	{
		DecoratorMemory->LastUseTimestamp = -FLT_MAX;
	}

	DecoratorMemory->bRequestedRestart = false;
}

void UBTDecorator_Cooldown::OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult)
{
	FBTCooldownDecoratorMemory* DecoratorMemory = GetNodeMemory<FBTCooldownDecoratorMemory>(SearchData);
	DecoratorMemory->LastUseTimestamp = SearchData.OwnerComp.GetWorld()->GetTimeSeconds();
	DecoratorMemory->bRequestedRestart = false;
}

void UBTDecorator_Cooldown::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTCooldownDecoratorMemory* DecoratorMemory = reinterpret_cast<FBTCooldownDecoratorMemory*>(NodeMemory);
	if (!DecoratorMemory->bRequestedRestart)
	{
		const float TimePassed = (OwnerComp.GetWorld()->GetTimeSeconds() - DecoratorMemory->LastUseTimestamp);
		if (TimePassed >= CoolDownTime)
		{
			DecoratorMemory->bRequestedRestart = true;
			OwnerComp.RequestExecution(this);
		}
	}
}

FString UBTDecorator_Cooldown::GetStaticDescription() const
{
	// basic info: result after time
	return FString::Printf(TEXT("%s: lock for %.1fs after execution and return %s"), *Super::GetStaticDescription(),
		CoolDownTime, *UBehaviorTreeTypes::DescribeNodeResult(EBTNodeResult::Failed));
}

void UBTDecorator_Cooldown::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	FBTCooldownDecoratorMemory* DecoratorMemory = reinterpret_cast<FBTCooldownDecoratorMemory*>(NodeMemory);
	const float TimePassed = OwnerComp.GetWorld()->GetTimeSeconds() - DecoratorMemory->LastUseTimestamp;
	
	if (TimePassed < CoolDownTime)
	{
		Values.Add(FString::Printf(TEXT("%s in %ss"),
			(FlowAbortMode == EBTFlowAbortMode::None) ? TEXT("unlock") : TEXT("restart"),
			*FString::SanitizeFloat(CoolDownTime - TimePassed)));
	}
}

uint16 UBTDecorator_Cooldown::GetInstanceMemorySize() const
{
	return sizeof(FBTCooldownDecoratorMemory);
}

#if WITH_EDITOR

FName UBTDecorator_Cooldown::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Cooldown.Icon");
}

#endif	// WITH_EDITOR
