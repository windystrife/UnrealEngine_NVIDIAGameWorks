// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Used by Preview in PhysicsAssetEditor, allows us to switch between immediate mode and vanilla physx
 */

#pragma once
#include "AnimPreviewInstance.h"
#include "PhysicsAssetEditorAnimInstance.generated.h"

class UAnimSequence;

UCLASS(transient, NotBlueprintable)
class UPhysicsAssetEditorAnimInstance : public UAnimPreviewInstance
{
	GENERATED_UCLASS_BODY()

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};



