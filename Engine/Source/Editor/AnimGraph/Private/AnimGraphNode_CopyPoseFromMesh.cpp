// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_CopyPoseFromMesh.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_CopyPoseFromMeshSkeletalControl

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_CopyPoseFromMesh::UAnimGraphNode_CopyPoseFromMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_CopyPoseFromMesh::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_CopyPoseFromMesh_Tooltip", "The Copy Pose From Mesh node copies the pose data from another component to this. Only works when name matches.");
}

FText UAnimGraphNode_CopyPoseFromMesh::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("CopyPoseFromMesh", "Copy Pose From Mesh");
}

#undef LOCTEXT_NAMESPACE
