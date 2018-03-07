// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateMaterialResource.h"
#include "Materials/MaterialInstanceDynamic.h"


FSlateMaterialResource::FSlateMaterialResource(const UMaterialInterface& InMaterial, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask )
	: MaterialObject( &InMaterial )
	, SlateProxy( new FSlateShaderResourceProxy )
	, TextureMaskResource( InTextureMask )
	, Width(FMath::RoundToInt(InImageSize.X))
	, Height(FMath::RoundToInt(InImageSize.Y))
{
	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;

#if !UE_BUILD_SHIPPING
	MaterialObjectWeakPtr = MaterialObject; 
	UpdateMaterialName();
#endif
}

FSlateMaterialResource::~FSlateMaterialResource()
{
	if(SlateProxy)
	{
		delete SlateProxy;
	}
}

void FSlateMaterialResource::UpdateMaterial(const UMaterialInterface& InMaterialResource, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask )
{
	MaterialObject = &InMaterialResource;

#if !UE_BUILD_SHIPPING
	MaterialObjectWeakPtr = MaterialObject;
	UpdateMaterialName();
#endif

	if( !SlateProxy )
	{
		SlateProxy = new FSlateShaderResourceProxy;
	}

	TextureMaskResource = InTextureMask;

	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;

	Width = FMath::RoundToInt(InImageSize.X);
	Height = FMath::RoundToInt(InImageSize.Y);
}

void FSlateMaterialResource::ResetMaterial()
{
	MaterialObject = nullptr;

#if !UE_BUILD_SHIPPING
	MaterialObjectWeakPtr = nullptr;
	UpdateMaterialName();
#endif

	TextureMaskResource = nullptr;
	if (SlateProxy)
	{
		delete SlateProxy;
	}
	SlateProxy = nullptr;
	Width = 0;
	Height = 0;
}

#if !UE_BUILD_SHIPPING
void FSlateMaterialResource::UpdateMaterialName()
{
	const UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialObject);
	if(MID && MID->Parent)
	{
		// MID's don't have nice names. Get the name of the parent instead for tracking
		MaterialName = MID->Parent->GetFName();
	}
	else if(MaterialObject)
	{
		MaterialName = MaterialObject->GetFName();
	}
	else
	{
		MaterialName = NAME_None;
	}
}
#endif
