// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimStateNode.cpp
=============================================================================*/

#include "AnimStateEntryNode.h"
#include "EdGraph/EdGraph.h"
#include "AnimationStateMachineSchema.h"

#define LOCTEXT_NAMESPACE "AnimStateEntryNode"

/////////////////////////////////////////////////////
// UAnimStateEntryNode

UAnimStateEntryNode::UAnimStateEntryNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimStateEntryNode::AllocateDefaultPins()
{
	const UAnimationStateMachineSchema* Schema = GetDefault<UAnimationStateMachineSchema>();
	UEdGraphPin* Outputs = CreatePin(EGPD_Output, Schema->PC_Exec, FString(), nullptr, TEXT("Entry"));
}

FText UAnimStateEntryNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEdGraph* Graph = GetGraph();
	return FText::FromString(Graph->GetName());
}

FText UAnimStateEntryNode::GetTooltipText() const
{
	return LOCTEXT("StateEntryNodeTooltip", "Entry point for state machine");
}

UEdGraphNode* UAnimStateEntryNode::GetOutputNode() const
{
	if(Pins.Num() > 0 && Pins[0] != NULL)
	{
		check(Pins[0]->LinkedTo.Num() <= 1);
		if(Pins[0]->LinkedTo.Num() > 0 && Pins[0]->LinkedTo[0]->GetOwningNode() != NULL)
		{
			return Pins[0]->LinkedTo[0]->GetOwningNode();
		}
	}
	return NULL;
}

#undef LOCTEXT_NAMESPACE
