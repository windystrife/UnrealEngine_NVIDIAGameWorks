// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_LiveLinkPose.h"

#define LOCTEXT_NAMESPACE "LiveLinkAnimNode"

UAnimGraphNode_LiveLinkPose::UAnimGraphNode_LiveLinkPose(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_LiveLinkPose::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Live Link Pose");
}

FText UAnimGraphNode_LiveLinkPose::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Retrieves the current pose associated with the supplied subject");
}

FText UAnimGraphNode_LiveLinkPose::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Live Link");
}

#undef LOCTEXT_NAMESPACE