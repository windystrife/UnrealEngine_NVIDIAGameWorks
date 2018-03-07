// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"

#include "ParticleEventManager.generated.h"

UCLASS(config=Game, NotBlueprintable, notplaceable)
class AParticleEventManager : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void HandleParticleSpawnEvents( UParticleSystemComponent* Component, const TArray<FParticleEventSpawnData>& SpawnEvents );
	virtual void HandleParticleDeathEvents( UParticleSystemComponent* Component, const TArray<FParticleEventDeathData>& DeathEvents );
	virtual void HandleParticleCollisionEvents( UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& CollisionEvents );
	virtual void HandleParticleBurstEvents( UParticleSystemComponent* Component, const TArray<FParticleEventBurstData>& BurstEvents );
};

