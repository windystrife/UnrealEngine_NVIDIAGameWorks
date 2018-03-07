// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraLightRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraConstants.h"

UNiagaraLightRendererProperties::UNiagaraLightRendererProperties()
	: RadiusScale(1.0f), ColorAdd(FVector(0.0f, 0.0f, 0.0f))
{
}

NiagaraRenderer* UNiagaraLightRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel)
{
	return new NiagaraRendererLights(FeatureLevel, this);
}

void UNiagaraLightRendererProperties::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	//OutMaterials.Add(Material);
	//Material should live here.
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& UNiagaraLightRendererProperties::GetRequiredAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_COLOR);
		Attrs.Add(SYS_PARAM_PARTICLES_SCALE);
		/*Attrs.Add(SYS_PARAM_PARTICLES_VELOCITY);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		Attrs.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE);*/
	}

	return Attrs;
}

const TArray<FNiagaraVariable>& UNiagaraLightRendererProperties::GetOptionalAttributes()
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

bool UNiagaraLightRendererProperties::IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage)
{
	return true;
}

void UNiagaraLightRendererProperties::FixMaterial(UMaterial* Material)
{
}

#endif // WITH_EDITORONLY_DATA

