// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_TagCooldown.h"
#include "Engine/World.h"

UBTDecorator_TagCooldown::UBTDecorator_TagCooldown(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Tag Cooldown";
	CooldownDuration = 5.0f;
	bAddToExistingDuration = false;
	bActivatesCooldown = true;
	
	// aborting child nodes doesn't makes sense, cooldown starts after leaving this branch
	bAllowAbortChildNodes = false;

	bNotifyTick = false;
	bNotifyDeactivation = true;
}

void UBTDecorator_TagCooldown::PostLoad()
{
	Super::PostLoad();
	bNotifyTick = (FlowAbortMode != EBTFlowAbortMode::None);
}

bool UBTDecorator_TagCooldown::HasCooldownFinished(const UBehaviorTreeComponent& OwnerComp) const
{
	const float TagCooldownEndTime = OwnerComp.GetTagCooldownEndTime(CooldownTag);

	if (TagCooldownEndTime == 0.f)
	{
		// special case, we don't have an end time yet for this cooldown tag
		return true;
	}

	return (OwnerComp.GetWorld()->GetTimeSeconds() >= TagCooldownEndTime);
}

bool UBTDecorator_TagCooldown::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	return HasCooldownFinished(OwnerComp);
}

void UBTDecorator_TagCooldown::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	FBTTagCooldownDecoratorMemory* DecoratorMemory = (FBTTagCooldownDecoratorMemory*)NodeMemory;

	DecoratorMemory->bRequestedRestart = false;
}

void UBTDecorator_TagCooldown::OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult)
{
	FBTTagCooldownDecoratorMemory* DecoratorMemory = GetNodeMemory<FBTTagCooldownDecoratorMemory>(SearchData);
	DecoratorMemory->bRequestedRestart = false;

	if (bActivatesCooldown)
	{
		SearchData.OwnerComp.AddCooldownTagDuration(CooldownTag, CooldownDuration, bAddToExistingDuration);
	}
}

void UBTDecorator_TagCooldown::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTTagCooldownDecoratorMemory* DecoratorMemory = (FBTTagCooldownDecoratorMemory*)NodeMemory;
	if (!DecoratorMemory->bRequestedRestart)
	{
		if (HasCooldownFinished(OwnerComp))
		{
			DecoratorMemory->bRequestedRestart = true;
			OwnerComp.RequestExecution(this);
		}
	}
}

FString UBTDecorator_TagCooldown::GetStaticDescription() const
{
	// basic info: result after time
	return FString::Printf(TEXT("%s %s: lock for %.1fs after execution and return %s"), *Super::GetStaticDescription(), *CooldownTag.ToString(), CooldownDuration, *UBehaviorTreeTypes::DescribeNodeResult(EBTNodeResult::Failed));
}

void UBTDecorator_TagCooldown::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);
	
	const float TagCooldownEndTime = OwnerComp.GetTagCooldownEndTime(CooldownTag);

	// if the tag cooldown end time is 0.f then it hasn't been set yet.
	if (TagCooldownEndTime > 0.f)
	{
		const float TimePassed = (OwnerComp.GetWorld()->GetTimeSeconds() - TagCooldownEndTime);

		if (TimePassed < CooldownDuration)
		{
			Values.Add(FString::Printf(TEXT("%s in %ss"),
				(FlowAbortMode == EBTFlowAbortMode::None) ? TEXT("unlock") : TEXT("restart"),
				*FString::SanitizeFloat(CooldownDuration - TimePassed)));
		}
	}
}

uint16 UBTDecorator_TagCooldown::GetInstanceMemorySize() const
{
	return sizeof(FBTTagCooldownDecoratorMemory);
}

#if WITH_EDITOR

FName UBTDecorator_TagCooldown::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Cooldown.Icon");
}

#endif	// WITH_EDITOR
