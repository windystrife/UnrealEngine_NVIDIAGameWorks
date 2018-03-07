// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_ConditionalLoop.h"
#include "GameFramework/Actor.h"
#include "VisualLogger/VisualLogger.h"
#include "BehaviorTree/BTCompositeNode.h"

UBTDecorator_ConditionalLoop::UBTDecorator_ConditionalLoop(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Conditional Loop";
	bNotifyDeactivation = true;

	bAllowAbortNone = false;
	bAllowAbortLowerPri = false;
	bAllowAbortChildNodes = false;
}

bool UBTDecorator_ConditionalLoop::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	// always allows execution
	return true;
}

EBlackboardNotificationResult UBTDecorator_ConditionalLoop::OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	// empty, don't react to blackboard value changes
	return EBlackboardNotificationResult::RemoveObserver;
}

void UBTDecorator_ConditionalLoop::OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult)
{
	if (NodeResult != EBTNodeResult::Aborted)
	{
		const UBlackboardComponent* BlackboardComp = SearchData.OwnerComp.GetBlackboardComponent();
		const bool bEvalResult = BlackboardComp && EvaluateOnBlackboard(*BlackboardComp);
		UE_VLOG(SearchData.OwnerComp.GetOwner(), LogBehaviorTree, Verbose, TEXT("Loop condition: %s -> %s"),
			bEvalResult ? TEXT("true") : TEXT("false"), (bEvalResult != IsInversed()) ? TEXT("run again!") : TEXT("break"));

		if (bEvalResult != IsInversed())
		{
			GetParentNode()->SetChildOverride(SearchData, GetChildIndex());
		}
	}
}

#if WITH_EDITOR

FName UBTDecorator_ConditionalLoop::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Loop.Icon");
}

#endif	// WITH_EDITOR
