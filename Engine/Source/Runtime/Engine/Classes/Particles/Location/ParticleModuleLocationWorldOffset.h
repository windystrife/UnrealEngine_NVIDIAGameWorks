// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "ParticleModuleLocationWorldOffset.generated.h"

struct FParticleEmitterInstance;

UCLASS(editinlinenew, meta=(DisplayName = "World Offset"))
class ENGINE_API UParticleModuleLocationWorldOffset : public UParticleModuleLocation
{
	GENERATED_UCLASS_BODY()


protected:
	//Begin UParticleModuleLocation Interface
	virtual void SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, struct FRandomStream* InRandomStream, FBaseParticle* ParticleBase) override;
	//End UParticleModuleLocation Interface
};

