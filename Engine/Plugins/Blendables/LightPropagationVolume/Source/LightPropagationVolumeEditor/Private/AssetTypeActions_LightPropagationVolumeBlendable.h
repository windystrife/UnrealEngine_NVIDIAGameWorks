// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "LightPropagationVolumeBlendable.h"

class FAssetTypeActions_LightPropagationVolumeBlendable : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_LightPropagationVolumeBlendable(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{
	}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_LightPropagationVolumeBlendable", "LightPropagationVolume Blendable"); }
	virtual FColor GetTypeColor() const override { return FColor(128, 0, 200); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return false; }
	virtual UClass* GetSupportedClass() const override { return ULightPropagationVolumeBlendable::StaticClass(); }
	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override { return AssetCategoryBit; }

private:

	EAssetTypeCategories::Type AssetCategoryBit;
};
