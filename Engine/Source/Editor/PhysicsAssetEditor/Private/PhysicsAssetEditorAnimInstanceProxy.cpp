// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditorAnimInstanceProxy.h"
#include "PhysicsAssetEditorAnimInstance.h"
//#include "AnimNode_RigidBody.h"

void FPhysicsAssetEditorAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimPreviewInstanceProxy::Initialize(InAnimInstance);
	ConstructNodes();
}

void FPhysicsAssetEditorAnimInstanceProxy::ConstructNodes()
{
	//LocalToComponentSpace.LocalPose.SetLinkNode(&SequencePlayerNode);
	//RagdollNode.ComponentPose.SetLinkNode(&LocalToComponentSpace);
	//ComponentToLocalSpace.ComponentPose.SetLinkNode(&RagdollNode);
	//ComponentToLocalSpace.ComponentPose.SetLinkNode(&LocalToComponentSpace);
	
	//RagdollNode.SimulationSpace = ESimulationSpace::WorldSpace;
}

void FPhysicsAssetEditorAnimInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	//OutNodes.Add(&RagdollNode);
}

FPhysicsAssetEditorAnimInstanceProxy::~FPhysicsAssetEditorAnimInstanceProxy()
{
}