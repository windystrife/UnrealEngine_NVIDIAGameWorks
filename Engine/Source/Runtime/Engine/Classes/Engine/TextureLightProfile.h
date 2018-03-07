// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture2D.h"
#include "TextureLightProfile.generated.h"

UCLASS(hidecategories=(Object,CompositeTexture,Texture2D), MinimalAPI, BlueprintType)
class UTextureLightProfile : public UTexture2D
{
	GENERATED_UCLASS_BODY()

	/** Light brightness in Lumens, imported from IES profile, <= 0 if the profile is used for masking only. Use with InverseSquareFalloff. */
	UPROPERTY(EditAnywhere, Category=TextureLightProfile, AssetRegistrySearchable)
	float Brightness;

	/** Multiplier to map texture value to result to integrate over the sphere to 1.0f */
	UPROPERTY(VisibleAnywhere, Category=TextureLightProfile)
	float TextureMultiplier;
};
