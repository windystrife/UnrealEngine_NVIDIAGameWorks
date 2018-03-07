// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_CustomTransitionResult.h"
#include "GraphEditorSettings.h"


/////////////////////////////////////////////////////
// UAnimGraphNode_CustomTransitionResult

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_CustomTransitionResult::UAnimGraphNode_CustomTransitionResult(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_CustomTransitionResult::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText UAnimGraphNode_CustomTransitionResult::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_CustomTransitionResult_Tooltip", "Result node for a custom transition blend graph");
}

FText UAnimGraphNode_CustomTransitionResult::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_CustomTransitionResult_Title", "Custom Transition Blend Result");
}

#undef LOCTEXT_NAMESPACE
