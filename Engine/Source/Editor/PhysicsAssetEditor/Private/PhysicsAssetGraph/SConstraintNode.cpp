// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SConstraintNode.h"
#include "PhysicsAssetGraphNode_Constraint.h"
#include "SGraphPin.h"

void SConstraintNode::Construct(const FArguments& InArgs, UPhysicsAssetGraphNode_Constraint* InNode)
{
	SPhysicsAssetGraphNode::Construct(SPhysicsAssetGraphNode::FArguments(), InNode);

	UpdateGraphNode();
}
