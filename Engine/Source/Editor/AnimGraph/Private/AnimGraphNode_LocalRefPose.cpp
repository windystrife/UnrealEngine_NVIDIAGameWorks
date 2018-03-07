// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_LocalRefPose.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_LocalRefPose

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_LocalRefPose::UAnimGraphNode_LocalRefPose(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Node.RefPoseType = EIT_LocalSpace;
}

FText UAnimGraphNode_LocalRefPose::GetTooltipText() const
{
	return LOCTEXT("UAnimGraphNode_LocalRefPose_Tooltip", "Returns local space reference pose.");
}

FText UAnimGraphNode_LocalRefPose::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UAnimGraphNode_LocalRefPose_Title", "Local Space Ref Pose");
}

#undef LOCTEXT_NAMESPACE
