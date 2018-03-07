// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_RotateRootBone.h"


/////////////////////////////////////////////////////
// UAnimGraphNode_RotateRootBone

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_RotateRootBone::UAnimGraphNode_RotateRootBone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_RotateRootBone::GetNodeTitleColor() const
{
	return FLinearColor(0.7f, 0.7f, 0.7f);
}

FText UAnimGraphNode_RotateRootBone::GetTooltipText() const
{
	return LOCTEXT("RotateRootBone", "Rotate Root Bone");
}

FText UAnimGraphNode_RotateRootBone::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("RotateRootBone", "Rotate Root Bone");
}

FString UAnimGraphNode_RotateRootBone::GetNodeCategory() const
{
	return TEXT("Tools");
}

#undef LOCTEXT_NAMESPACE
