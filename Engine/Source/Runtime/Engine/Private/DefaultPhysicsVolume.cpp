// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/DefaultPhysicsVolume.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Components/BrushComponent.h"

ADefaultPhysicsVolume::ADefaultPhysicsVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

#if WITH_EDITORONLY_DATA
	// Not allowed to be selected or edited within Unreal Editor
	bEditable = false;
#endif // WITH_EDITORONLY_DATA

	// update default values when world is restarted
	TerminalVelocity = UPhysicsSettings::Get()->DefaultTerminalVelocity;
	FluidFriction = UPhysicsSettings::Get()->DefaultFluidFriction;

	// DefaultPhysicsVolumes are spawned only as a fallback object when determining the current physics volume.
	// They are not intended to actually have any collision response, as they don't have actual collision geometry.
	GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
