// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SBoneNode.h"
#include "PhysicsAssetGraphNode_Bone.h"
#include "SGraphPin.h"
#include "SInlineEditableTextBlock.h"
#include "PhysicsEngine/ShapeElem.h"
#include "SImage.h"
#include "SButton.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "SBoneNode"

void SBoneNode::Construct(const FArguments& InArgs, UPhysicsAssetGraphNode_Bone* InNode)
{
	SPhysicsAssetGraphNode::Construct(SPhysicsAssetGraphNode::FArguments(), InNode);
	UpdateGraphNode();
}

#undef LOCTEXT_NAMESPACE