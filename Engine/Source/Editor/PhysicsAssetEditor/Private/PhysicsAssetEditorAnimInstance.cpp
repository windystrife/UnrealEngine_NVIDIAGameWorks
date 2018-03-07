// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditorAnimInstance.h"
#include "PhysicsAssetEditorAnimInstanceProxy.h"

/////////////////////////////////////////////////////
// UPhysicsAssetEditorAnimInstance
/////////////////////////////////////////////////////

UPhysicsAssetEditorAnimInstance::UPhysicsAssetEditorAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = true;
}

FAnimInstanceProxy* UPhysicsAssetEditorAnimInstance::CreateAnimInstanceProxy()
{
	return new FPhysicsAssetEditorAnimInstanceProxy(this);
}
