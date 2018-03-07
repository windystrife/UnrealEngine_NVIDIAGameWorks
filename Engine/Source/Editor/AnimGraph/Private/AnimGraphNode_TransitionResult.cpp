// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_TransitionResult.h"
#include "GraphEditorSettings.h"


/////////////////////////////////////////////////////
// UAnimGraphNode_TransitionResult

#define LOCTEXT_NAMESPACE "UAnimGraphNode_TransitionResult"

UAnimGraphNode_TransitionResult::UAnimGraphNode_TransitionResult(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_TransitionResult::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText UAnimGraphNode_TransitionResult::GetTooltipText() const
{
	return LOCTEXT("TransitionResultTooltip", "This expression is evaluated to determine if the state transition can be taken");
}

FText UAnimGraphNode_TransitionResult::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Result", "Result");
}

void UAnimGraphNode_TransitionResult::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Intentionally empty. This node is auto-generated when a transition graph is created.
}

#undef LOCTEXT_NAMESPACE
