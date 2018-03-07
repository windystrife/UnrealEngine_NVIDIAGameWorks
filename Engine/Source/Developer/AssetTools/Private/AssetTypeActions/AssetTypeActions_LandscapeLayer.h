// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "LandscapeLayerInfoObject.h"

class FAssetTypeActions_LandscapeLayer : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_LandscapeLayer", "Landscape Layer"); }
	virtual FColor GetTypeColor() const override { return FColor(128,192,255); }
	virtual UClass* GetSupportedClass() const override { return ULandscapeLayerInfoObject::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};
