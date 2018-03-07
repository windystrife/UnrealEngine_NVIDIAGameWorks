// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DecalComponentVisualizer.h"
#include "SceneManagement.h"
#include "Components/DecalComponent.h"



void FDecalComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	const UDecalComponent* DecalComponent = Cast<const UDecalComponent>(Component);
	if(DecalComponent)
	{
		const FMatrix LocalToWorld = DecalComponent->GetComponentTransform().ToMatrixWithScale();
		
		const FLinearColor DrawColor = FColor(0, 157, 0, 255);

		DrawOrientedWireBox(PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetScaledAxis( EAxis::X ), LocalToWorld.GetScaledAxis( EAxis::Y ), LocalToWorld.GetScaledAxis( EAxis::Z ), DecalComponent->DecalSize, DrawColor, SDPG_World);
	}
}
