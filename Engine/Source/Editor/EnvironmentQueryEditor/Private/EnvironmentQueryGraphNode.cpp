// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryGraphNode.h"
#include "EdGraphSchema_EnvironmentQuery.h"
#include "EnvironmentQueryGraph.h"

UEnvironmentQueryGraphNode::UEnvironmentQueryGraphNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FText UEnvironmentQueryGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::GetEmpty();
}

FText UEnvironmentQueryGraphNode::GetDescription() const
{
	return FText::GetEmpty();
}

UEnvironmentQueryGraph* UEnvironmentQueryGraphNode::GetEnvironmentQueryGraph()
{
	return CastChecked<UEnvironmentQueryGraph>(GetGraph());
}

bool UEnvironmentQueryGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UEdGraphSchema_EnvironmentQuery::StaticClass());
}
