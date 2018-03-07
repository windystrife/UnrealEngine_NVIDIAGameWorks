// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_SubInput.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GraphEditorSettings.h"
#include "BlueprintActionFilter.h"

#define LOCTEXT_NAMESPACE "SubInputNode"

FLinearColor UAnimGraphNode_SubInput::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText UAnimGraphNode_SubInput::GetTooltipText() const
{
	return LOCTEXT("ToolTip", "Inputs to a sub-animation graph from a parent instance.");
}

FText UAnimGraphNode_SubInput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Sub-Graph Input");
}

bool UAnimGraphNode_SubInput::CanUserDeleteNode() const
{
	return true;
}

bool UAnimGraphNode_SubInput::CanDuplicateNode() const
{
	return false;
}

bool UAnimGraphNode_SubInput::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	bool bFilterOut = false;

	TArray<UAnimGraphNode_SubInput*> SubInputNodes;

	for(UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		SubInputNodes.Empty(SubInputNodes.Num());

		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, SubInputNodes);

		if(SubInputNodes.Num() > 0)
		{
			bFilterOut = true;
			break;
		}
	}

	return bFilterOut;
}

#undef LOCTEXT_NAMESPACE
