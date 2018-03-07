// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTCompositeNode.h"

UBehaviorTreeGraphNode_Decorator::UBehaviorTreeGraphNode_Decorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsSubNode = true;
}

void UBehaviorTreeGraphNode_Decorator::AllocateDefaultPins()
{
	//No Pins for decorators
}

FText UBehaviorTreeGraphNode_Decorator::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UBTDecorator* Decorator = Cast<UBTDecorator>(NodeInstance);
	if (Decorator != NULL)
	{
		return FText::FromString(Decorator->GetNodeName());
	}
	else if (!ClassData.GetClassName().IsEmpty())
	{
		FString StoredClassName = ClassData.GetClassName();
		StoredClassName.RemoveFromEnd(TEXT("_C"));

		return FText::Format(NSLOCTEXT("AIGraph", "NodeClassError", "Class {0} not found, make sure it's saved!"), FText::FromString(StoredClassName));
	}

	return Super::GetNodeTitle(TitleType);
}

void UBehaviorTreeGraphNode_Decorator::CollectDecoratorData(TArray<UBTDecorator*>& NodeInstances, TArray<FBTDecoratorLogic>& Operations) const
{
	if (NodeInstance)
	{
		UBTDecorator* DecoratorNode = (UBTDecorator*)NodeInstance;
		const int32 InstanceIdx = NodeInstances.Add(DecoratorNode);
		Operations.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, InstanceIdx));
	}
}
