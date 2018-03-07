// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/Material/ParticleModuleMaterialBase.h"
#include "ParticleModuleMeshMaterial.generated.h"

class UParticleLODLevel;
struct FParticleEmitterInstance;

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "Mesh Material"))
class UParticleModuleMeshMaterial : public UParticleModuleMaterialBase
{
	GENERATED_UCLASS_BODY()

	/** The array of materials to apply to the mesh particles. */
	UPROPERTY(EditAnywhere, Category=MeshMaterials)
	TArray<class UMaterialInterface*> MeshMaterials;


	//Begin UParticleModule Interface
	virtual void	Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual uint32	RequiredBytesPerInstance() override;

#if WITH_EDITOR
	virtual bool	IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString) override;
#endif
	//End UParticleModule Interface
};
