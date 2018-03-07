// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleModuleVectorFieldRotationRate: Rotation rate for vector fields.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/VectorField/ParticleModuleVectorFieldBase.h"
#include "ParticleModuleVectorFieldRotationRate.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "VF Rotation Rate"))
class UParticleModuleVectorFieldRotationRate : public UParticleModuleVectorFieldBase
{
	GENERATED_UCLASS_BODY()

	/** Constant rotation rate applied to the local vector field. */
	UPROPERTY(EditAnywhere, Category=VectorField)
	FVector RotationRate;


	//~ Begin UParticleModule Interface
	virtual void CompileModule(struct FParticleEmitterBuildInfo& EmitterInfo) override;
	//~ Begin UParticleModule Interface
};

