// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_ForceSuccess.h"
#include "GameFramework/Actor.h"
#include "VisualLogger/VisualLogger.h"

UBTDecorator_ForceSuccess::UBTDecorator_ForceSuccess(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = TEXT("Force Success");
	bNotifyProcessed = true;

	bAllowAbortNone = false;
	bAllowAbortLowerPri = false;
	bAllowAbortChildNodes = false;
}

void UBTDecorator_ForceSuccess::OnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult)
{
	NodeResult = EBTNodeResult::Succeeded;
	BT_SEARCHLOG(SearchData, Log, TEXT("Forcing Success: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(this));
}

#if WITH_EDITOR

FName UBTDecorator_ForceSuccess::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.ForceSuccess.Icon");
}

#endif	// WITH_EDITOR
