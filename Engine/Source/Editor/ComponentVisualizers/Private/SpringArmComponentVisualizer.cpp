// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SpringArmComponentVisualizer.h"
#include "SceneManagement.h"
#include "GameFramework/SpringArmComponent.h"

static const FColor	ArmColor(255,0,0);

void FSpringArmComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (const USpringArmComponent* SpringArm = Cast<const USpringArmComponent>(Component))
	{
		PDI->DrawLine( SpringArm->GetComponentLocation(), SpringArm->GetSocketTransform(TEXT("SpringEndpoint"),RTS_World).GetTranslation(), ArmColor, SDPG_World );
	}
}
