// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationNodes/SGraphNodeStateMachineInstance.h"
#include "AnimationStateMachineGraph.h"
#include "AnimGraphNode_StateMachineBase.h"

/////////////////////////////////////////////////////
// SGraphNodeStateMachineInstance

void SGraphNodeStateMachineInstance::Construct(const FArguments& InArgs, UAnimGraphNode_StateMachineBase* InNode)
{
	GraphNode = InNode;

	SetCursor(EMouseCursor::CardinalCross);

	UpdateGraphNode();
}

UEdGraph* SGraphNodeStateMachineInstance::GetInnerGraph() const
{
	UAnimGraphNode_StateMachineBase* StateMachineInstance = CastChecked<UAnimGraphNode_StateMachineBase>(GraphNode);

	return StateMachineInstance->EditorStateMachineGraph;
}
