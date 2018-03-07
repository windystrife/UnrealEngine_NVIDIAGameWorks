// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTreeEditorTypes.h"

UBehaviorTreeGraphNode_Task::UBehaviorTreeGraphNode_Task(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UBehaviorTreeGraphNode_Task::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UBehaviorTreeEditorTypes::PinCategory_SingleComposite, FString(), nullptr, TEXT("In"));
}

FText UBehaviorTreeGraphNode_Task::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UBTNode* MyNode = Cast<UBTNode>(NodeInstance);
	if (MyNode != NULL)
	{
		return FText::FromString(MyNode->GetNodeName());
	}
	else if (!ClassData.GetClassName().IsEmpty())
	{
		FString StoredClassName = ClassData.GetClassName();
		StoredClassName.RemoveFromEnd(TEXT("_C"));

		return FText::Format(NSLOCTEXT("AIGraph", "NodeClassError", "Class {0} not found, make sure it's saved!"), FText::FromString(StoredClassName));
	}

	return Super::GetNodeTitle(TitleType);
}

void UBehaviorTreeGraphNode_Task::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	AddContextMenuActionsDecorators(Context);
	AddContextMenuActionsServices(Context);
}
