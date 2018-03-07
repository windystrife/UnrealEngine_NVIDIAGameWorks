// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "LandscapeGrassType.h"

class FAssetTypeActions_LandscapeGrassType : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_LandscapeGrassType", "Landscape Grass Type"); }
	virtual FColor GetTypeColor() const override { return FColor(128,255,128); }
	virtual UClass* GetSupportedClass() const override { return ULandscapeGrassType::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};
