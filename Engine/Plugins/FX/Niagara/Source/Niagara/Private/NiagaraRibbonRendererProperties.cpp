// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraRendererRibbons.h"
#include "NiagaraConstants.h"

NiagaraRenderer* UNiagaraRibbonRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel)
{
	return new NiagaraRendererRibbons(FeatureLevel, this);
}

void UNiagaraRibbonRendererProperties::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	OutMaterials.Add(Material);
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& UNiagaraRibbonRendererProperties::GetRequiredAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		/*Attrs.Add(SYS_PARAM_PARTICLES_VELOCITY);
		Attrs.Add(SYS_PARAM_PARTICLES_COLOR);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		Attrs.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE);*/
	}

	return Attrs;
}


const TArray<FNiagaraVariable>& UNiagaraRibbonRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		//Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_FACING);
		//Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		//Attrs.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX);
		//Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
	}

	return Attrs;
}




bool UNiagaraRibbonRendererProperties::IsMaterialValidForRenderer(UMaterial* InMaterial, FText& InvalidMessage)
{
	if (InMaterial->bUsedWithNiagaraRibbons == false)
	{
		InvalidMessage = NSLOCTEXT("NiagaraRibbonRendererProperties", "InvalidMaterialMessage", "The material isn't marked as \"Used with Niagara ribbons\"");
		return false;
	}
	return true;
}

void UNiagaraRibbonRendererProperties::FixMaterial(UMaterial* InMaterial)
{
	InMaterial->Modify();
	InMaterial->bUsedWithNiagaraRibbons = true;
	InMaterial->ForceRecompileForRendering();
}

#endif // WITH_EDITORONLY_DATA
