// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "RHIDefinitions.h"
#include "NiagaraTypes.h"
#include "NiagaraRendererProperties.generated.h"
/**
* Emitter properties base class
* Each EmitterRenderer derives from this with its own class, and returns it in GetProperties; a copy
* of those specific properties is stored on UNiagaraEmitter (on the System) for serialization 
* and handed back to the System renderer on load.
*/

class NiagaraRenderer;
class UMaterial;
class UMaterialInterface;

UCLASS(ABSTRACT)
class NIAGARA_API UNiagaraRendererProperties : public UObject
{
	GENERATED_BODY()

public:
	virtual NiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel) PURE_VIRTUAL ( UNiagaraRendererProperties::CreateEmitterRenderer, return nullptr;);
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const PURE_VIRTUAL(UNiagaraRendererProperties::GetUsedMaterials,);

#if WITH_EDITORONLY_DATA
	virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) { return true; }

	virtual void FixMaterial(UMaterial* Material) { }

	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() { static TArray<FNiagaraVariable> Vars; return Vars; };
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() { static TArray<FNiagaraVariable> Vars; return Vars; };
#endif // WITH_EDITORONLY_DATA
};


