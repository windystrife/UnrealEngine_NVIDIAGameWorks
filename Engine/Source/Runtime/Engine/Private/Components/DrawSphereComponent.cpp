// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/DrawSphereComponent.h"
#include "Engine/CollisionProfile.h"

UDrawSphereComponent::UDrawSphereComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bHiddenInGame = true;
	bUseEditorCompositing = true;
	bGenerateOverlapEvents = false;
}

#if WITH_EDITOR
bool UDrawSphereComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	// Draw sphere components not treated as 'selectable' in editor
	return false;
}

bool UDrawSphereComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	// Draw sphere components not treated as 'selectable' in editor
	return false;
}
#endif
