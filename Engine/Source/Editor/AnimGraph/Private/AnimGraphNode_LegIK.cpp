// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_LegIK.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_LegIK

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_LegIK::UAnimGraphNode_LegIK(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_LegIK::GetControllerDescription() const
{
	return LOCTEXT("LegIK", "Leg IK");
}

FText UAnimGraphNode_LegIK::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_LegIK_Tooltip", "IK node for multi-bone legs.");
}

FText UAnimGraphNode_LegIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}


#undef LOCTEXT_NAMESPACE
