// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureLightProfile.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"

class FAssetTypeActions_TextureLightProfile : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureLightProfile", "Texture Light Profile"); }
	virtual FColor GetTypeColor() const override { return FColor(192,64,192); }
	virtual UClass* GetSupportedClass() const override { return UTextureLightProfile::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
