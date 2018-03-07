// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * this volume only blocks the path builder - it has no gameplay collision
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavigationTypes.h"
#include "GameFramework/Volume.h"
#include "NavMeshBoundsVolume.generated.h"

UCLASS(MinimalAPI)
class ANavMeshBoundsVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Navigation)
	FNavAgentSelector SupportedAgents;

	//~ Begin AActor Interface
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
	//~ End AActor Interface
#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	//~ End UObject Interface
#endif // WITH_EDITOR
};



