// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/**
 *	ParticleModuleFlexShapeSpawn
 *
 *	Given a static mesh reference (with a flex asset) it will spawn particles in solid configuration
 *
 */

#pragma once

#include "Particles/Spawn/ParticleModuleSpawnBase.h"
#include "ParticleModuleFlexShapeSpawn.generated.h"

UCLASS(editinlinenew, hidecategories = Object, MinimalAPI, meta = (DisplayName = "Spawn Flex Shape"))
class UParticleModuleFlexShapeSpawn : public UParticleModuleSpawnBase
{
	GENERATED_UCLASS_BODY()
	
	/** The mesh volume to emit from */
	UPROPERTY(EditAnywhere, Category=Spawn)
	UStaticMesh* Mesh;
	
	/** Velocity to emit particles with, note that this increases the required spawn rate */
	UPROPERTY(EditAnywhere, Category=Velocity)
	float Velocity;

	// Begin UParticleModuleSpawnBase Interface
	virtual bool	GetSpawnAmount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number, float& Rate) override;
	virtual bool	GetBurstCount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number) override;
	virtual int32	GetMaximumBurstCount() override;
	// End UParticleModuleSpawnBase Interface

	// Begin UParticleModule Interface
	virtual void	Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void	Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	// End UParticleModule Interface
};



