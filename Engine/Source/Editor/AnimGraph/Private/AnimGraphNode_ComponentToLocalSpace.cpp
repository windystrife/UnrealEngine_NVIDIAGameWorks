// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_ComponentToLocalSpace.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_ComponentToLocalSpace

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_ComponentToLocalSpace::UAnimGraphNode_ComponentToLocalSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_ComponentToLocalSpace::GetNodeTitleColor() const
{
	return FLinearColor(0.7f, 0.7f, 0.7f);
}

FText UAnimGraphNode_ComponentToLocalSpace::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_ComponentToLocalSpace_Tooltip", "Convert Component Space Pose to Local Pose");
}

FText UAnimGraphNode_ComponentToLocalSpace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_ComponentToLocalSpace_Title", "Component To Local");
}

FString UAnimGraphNode_ComponentToLocalSpace::GetNodeCategory() const
{
	return TEXT("Convert Spaces");
}

void UAnimGraphNode_ComponentToLocalSpace::PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const
{
	DisplayName = TEXT("");
}

#undef LOCTEXT_NAMESPACE
