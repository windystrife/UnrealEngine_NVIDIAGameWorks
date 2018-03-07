// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_SubsurfaceProfile : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SubsurfaceProfile", "Subsurface Profile"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 196, 128); }
	virtual UClass* GetSupportedClass() const override;

	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
};
