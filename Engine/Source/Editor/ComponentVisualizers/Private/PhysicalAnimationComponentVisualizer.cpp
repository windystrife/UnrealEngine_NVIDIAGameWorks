// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicalAnimationComponentVisualizer.h"

#include "PhysicsEngine/PhysicalAnimationComponent.h"

void FPhysicsAnimationComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(const UPhysicalAnimationComponent* PhysAnimComp = Cast<const UPhysicalAnimationComponent>(Component))
	{
		PhysAnimComp->DebugDraw(PDI);
	}
}
