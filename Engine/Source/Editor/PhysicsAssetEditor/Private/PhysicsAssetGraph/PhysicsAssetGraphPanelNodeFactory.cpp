// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetGraphPanelNodeFactory.h"
#include "PhysicsAssetGraphNode_Bone.h"
#include "PhysicsAssetGraphNode_Constraint.h"
#include "SBoneNode.h"
#include "SConstraintNode.h"
#include "PhysicsAssetGraph.h"

TSharedPtr<SGraphNode> FPhysicsAssetGraphPanelNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (UPhysicsAssetGraphNode_Bone* BoneNode = Cast<UPhysicsAssetGraphNode_Bone>(Node))
	{
		CastChecked<UPhysicsAssetGraph>(Node->GetGraph())->RequestRefreshLayout(true);

		TSharedRef<SGraphNode> GraphNode = SNew(SBoneNode, BoneNode);
		GraphNode->SlatePrepass();
		BoneNode->SetDimensions(GraphNode->GetDesiredSize());
		return GraphNode;
	}
	else if (UPhysicsAssetGraphNode_Constraint* ConstraintNode = Cast<UPhysicsAssetGraphNode_Constraint>(Node))
	{
		CastChecked<UPhysicsAssetGraph>(Node->GetGraph())->RequestRefreshLayout(true);

		TSharedRef<SGraphNode> GraphNode = SNew(SConstraintNode, ConstraintNode);
		GraphNode->SlatePrepass();
		ConstraintNode->SetDimensions(GraphNode->GetDesiredSize());
		return GraphNode;
	}

	return nullptr;
}