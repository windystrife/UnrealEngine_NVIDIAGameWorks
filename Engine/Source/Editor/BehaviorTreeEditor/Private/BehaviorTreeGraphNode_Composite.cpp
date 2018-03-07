// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTree/BTNode.h"

UBehaviorTreeGraphNode_Composite::UBehaviorTreeGraphNode_Composite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UBehaviorTreeGraphNode_Composite::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UBTNode* MyNode = Cast<UBTNode>(NodeInstance);
	if (MyNode != NULL)
	{
		return FText::FromString(MyNode->GetNodeName());
	}
	return Super::GetNodeTitle(TitleType);
}

void UBehaviorTreeGraphNode_Composite::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	AddContextMenuActionsDecorators(Context);
	AddContextMenuActionsServices(Context);
}
