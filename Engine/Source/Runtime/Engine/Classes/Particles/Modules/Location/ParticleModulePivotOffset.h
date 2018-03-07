// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/Location/ParticleModuleLocationBase.h"
#include "ParticleModulePivotOffset.generated.h"

class UParticleLODLevel;

UCLASS(editinlinenew, hidecategories=Object, DisplayName="Pivot Offset")
class ENGINE_API UParticleModulePivotOffset : public UParticleModuleLocationBase
{
	GENERATED_UCLASS_BODY()

	/** Offset applied in UV space to the particle vertex positions. Defaults to (0.5,0.5) putting the pivot in the centre of the partilce. */
	UPROPERTY(EditAnywhere, Category=PivotOffset)
	FVector2D PivotOffset;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//Begin UObject Interface
	virtual void PostInitProperties() override;
	//End UObject Interface

	//Begin UParticleModule Interface
	virtual void CompileModule( struct FParticleEmitterBuildInfo& EmitterInfo ) override;
	//End UParticleModule Interface

#if WITH_EDITOR
	virtual bool IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString) override;
#endif

};



