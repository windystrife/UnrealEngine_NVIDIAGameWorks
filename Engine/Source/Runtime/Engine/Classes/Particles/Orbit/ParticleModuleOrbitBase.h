// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/ParticleModule.h"
#include "ParticleModuleOrbitBase.generated.h"

UCLASS(editinlinenew, hidecategories=Object, abstract, meta=(DisplayName = "Orbit"))
class UParticleModuleOrbitBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	If true, distribution values will be retrieved using the EmitterTime.
	 *	If false (default), they will be retrieved using the Particle.RelativeTime.
	 */
	UPROPERTY(EditAnywhere, Category=Orbit)
	uint32 bUseEmitterTime:1;

};

