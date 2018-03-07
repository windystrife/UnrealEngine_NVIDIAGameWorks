// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeGraphNode_SimpleParallel.h"
#include "BehaviorTreeEditorTypes.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeEditor"

UBehaviorTreeGraphNode_SimpleParallel::UBehaviorTreeGraphNode_SimpleParallel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UBehaviorTreeGraphNode_SimpleParallel::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UBehaviorTreeEditorTypes::PinCategory_MultipleNodes, FString(), nullptr, TEXT("In"));
	
	CreatePin(EGPD_Output, UBehaviorTreeEditorTypes::PinCategory_SingleTask, FString(), nullptr, TEXT("Task"));
	CreatePin(EGPD_Output, UBehaviorTreeEditorTypes::PinCategory_SingleNode, FString(), nullptr, TEXT("Out"));
}

void UBehaviorTreeGraphNode_SimpleParallel::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	ensure(Pin.GetOwningNode() == this);
	if (Pin.Direction == EGPD_Output)
	{
		if (Pin.PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleTask)
		{
			HoverTextOut = LOCTEXT("PinHoverParallelMain","Main task of parrallel node").ToString();
		}
		else
		{
			HoverTextOut = LOCTEXT("PinHoverParallelBackground","Nodes running in the background, while main task is active").ToString();
		}
	}
}

#undef LOCTEXT_NAMESPACE
