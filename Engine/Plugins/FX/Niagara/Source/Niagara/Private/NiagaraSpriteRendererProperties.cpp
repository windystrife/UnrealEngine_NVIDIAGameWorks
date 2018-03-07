// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRenderer.h"
#include "Internationalization.h"
#include "NiagaraConstants.h"

#define LOCTEXT_NAMESPACE "UNiagaraSpriteRendererProperties"

UNiagaraSpriteRendererProperties::UNiagaraSpriteRendererProperties()
	: SubImageSize(1.0f, 1.0f)
	, Alignment(ENiagaraSpriteAlignment::Unaligned)
	, FacingMode(ENiagaraSpriteFacingMode::FaceCamera)
	, CustomFacingVectorMask(EForceInit::ForceInitToZero)
	, SortMode(ENiagaraSortMode::SortNone)
{
}

NiagaraRenderer* UNiagaraSpriteRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel)
{
	return new NiagaraRendererSprites(FeatureLevel, this);
}

void UNiagaraSpriteRendererProperties::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	OutMaterials.Add(Material);
}


#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& UNiagaraSpriteRendererProperties::GetRequiredAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_VELOCITY);
		Attrs.Add(SYS_PARAM_PARTICLES_COLOR);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		Attrs.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE);
	}

	return Attrs;
}


const TArray<FNiagaraVariable>& UNiagaraSpriteRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_FACING);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		Attrs.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
	}

	return Attrs;
}


bool UNiagaraSpriteRendererProperties::IsMaterialValidForRenderer(UMaterial* InMaterial, FText& InvalidMessage)
{
	if (InMaterial->bUsedWithNiagaraSprites == false)
	{
		InvalidMessage = NSLOCTEXT("NiagaraSpriteRendererProperties", "InvalidMaterialMessage", "The material isn't marked as \"Used with particle sprites\"");
		return false;
	}
	return true;
}

void UNiagaraSpriteRendererProperties::FixMaterial(UMaterial* InMaterial)
{
	InMaterial->Modify();
	InMaterial->bUsedWithNiagaraSprites = true;
	InMaterial->ForceRecompileForRendering();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITORONLY_DATA




