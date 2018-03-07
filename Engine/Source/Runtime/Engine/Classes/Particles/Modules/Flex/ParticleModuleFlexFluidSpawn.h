// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/**
 *	ParticleModuleFlexFluidSpawn
 *
 *	Spawns particles at fluid rest density with appropriate velocity
 *
 */

#pragma once

#include "Particles/Spawn/ParticleModuleSpawnBase.h"
#include "ParticleModuleFlexFluidSpawn.generated.h"

UCLASS(editinlinenew, hidecategories = Object, MinimalAPI, meta = (DisplayName = "Spawn Flex Fluid"))
class UParticleModuleFlexFluidSpawn : public UParticleModuleSpawnBase
{
	GENERATED_UCLASS_BODY()

	struct InstancePayload
	{
		float LayerLeftOver;
		float TimeLeftOver;
		int32 NumParticles;
		int32 ParticleIndex;
	};

	/** The number of particles to emit horizontally  */
	UPROPERTY(EditAnywhere, Category=Spawn)
	struct FRawDistributionFloat DimX;

	/** The number of particles to emit vertically */
	UPROPERTY(EditAnywhere, Category=Spawn)
	struct FRawDistributionFloat DimY;

	/** Scales the number of emitted layers into z direction. Values higher than 1 can cause explosions.*/
	UPROPERTY(EditAnywhere, Category=Spawn)
	struct FRawDistributionFloat LayerScale;

	/** Velocity to emit particles with, note that this increases the required spawn rate */
	UPROPERTY(EditAnywhere, Category=Velocity)
	struct FRawDistributionFloat Velocity;

	// Begin UParticleModuleSpawnBase Interface
	virtual bool	GetSpawnAmount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number, float& Rate) override;
	virtual bool	GetBurstCount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number) override;
	virtual int32	GetMaximumBurstCount() override;
	// End UParticleModuleSpawnBase Interface

	// Begin UParticleModule Interface
	virtual uint32	RequiredBytesPerInstance() override;
	virtual uint32	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData) override;
	virtual void	Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void	Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void CompileModule(FParticleEmitterBuildInfo& EmitterInfo) override;
	virtual void PostInitProperties() override;
	// End UParticleModule Interface

	virtual void PostLoad() override;

	void InitializeDefaults();
};



