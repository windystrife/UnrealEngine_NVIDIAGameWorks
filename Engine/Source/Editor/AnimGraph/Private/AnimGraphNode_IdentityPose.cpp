// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_IdentityPose.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_IdentityPose

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_IdentityPose::UAnimGraphNode_IdentityPose(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Node.RefPoseType = EIT_Additive;
}

FText UAnimGraphNode_IdentityPose::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_IdentityPose_Tooltip", "Returns identity pose.");
}

FText UAnimGraphNode_IdentityPose::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_IdentityPose_Title", "Additive Identity Pose");
}

#undef LOCTEXT_NAMESPACE
