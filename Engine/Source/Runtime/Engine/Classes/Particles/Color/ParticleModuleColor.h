// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particles/Color/ParticleModuleColorBase.h"
#include "ParticleModuleColor.generated.h"

class UInterpCurveEdSetup;
class UParticleEmitter;
struct FCurveEdEntry;
struct FParticleEmitterInstance;

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Initial Color"))
class ENGINE_API UParticleModuleColor : public UParticleModuleColorBase
{
	GENERATED_UCLASS_BODY()

	/** Initial color for a particle as a function of Emitter time. */
	UPROPERTY(EditAnywhere, Category = Color, meta = (TreatAsColor))
	struct FRawDistributionVector StartColor;

	/** Initial alpha for a particle as a function of Emitter time. */
	UPROPERTY(EditAnywhere, Category=Color)
	struct FRawDistributionFloat StartAlpha;

	/** If true, the alpha value will be clamped to the [0..1] range. */
	UPROPERTY(EditAnywhere, Category=Color)
	uint32 bClampAlpha:1;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostInitProperties() override;
	//~ End UObject Interface


	//Begin UParticleModule Interface
	virtual	bool AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup, TArray<const FCurveEdEntry*>& OutCurveEntries) override;
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void CompileModule( struct FParticleEmitterBuildInfo& EmitterInfo ) override;
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) override;
	//End UParticleModule Interface

	/**
	 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
	 *
	 *	@param	Owner				The particle emitter instance that is spawning
	 *	@param	Offset				The offset to the modules payload data
	 *	@param	SpawnTime			The time of the spawn
	 *	@param	InRandomStream		The random stream to use for retrieving random values
	 */
	void SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, struct FRandomStream* InRandomStream, FBaseParticle* ParticleBase);

};



