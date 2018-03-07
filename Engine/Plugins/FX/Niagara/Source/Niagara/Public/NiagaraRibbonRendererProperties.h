// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRibbonRendererProperties.generated.h"

UCLASS(editinlinenew)
class UNiagaraRibbonRendererProperties : public UNiagaraRendererProperties
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Sprite Rendering")
	UMaterialInterface* Material;

	//~ UNiagaraRendererProperties interface
	virtual NiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;
#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
    virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) override;
	virtual void FixMaterial(UMaterial* Material);
#endif
};
