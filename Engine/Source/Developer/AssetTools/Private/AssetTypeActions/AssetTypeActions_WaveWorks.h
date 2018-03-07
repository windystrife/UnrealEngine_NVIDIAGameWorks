// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/WaveWorks.h"

class FAssetTypeActions_WaveWorks : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_WaveWorks", "WaveWorks"); }
	virtual FColor GetTypeColor() const override { return FColor(255,0,0); }
	virtual UClass* GetSupportedClass() const override{ return UWaveWorks::StaticClass(); }
	virtual uint32 GetCategories() override{ return EAssetTypeCategories::Physics; }
};