// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraRendererProperties.h"
#include "StaticMeshResources.h"
#include "NiagaraMeshRendererProperties.generated.h"

UCLASS(editinlinenew)
class UNiagaraMeshRendererProperties : public UNiagaraRendererProperties
{
public:
	GENERATED_BODY()

	UNiagaraMeshRendererProperties();

	//~ UNiagaraRendererProperties interface
	virtual NiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;

#if WITH_EDITORONLY_DATA
	virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) override;
	virtual void FixMaterial(UMaterial* Material) override;
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif // WITH_EDITORONLY_DATA

	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	UPROPERTY(EditAnywhere, Category = "Mesh Rendering")
	UStaticMesh *ParticleMesh;
};
