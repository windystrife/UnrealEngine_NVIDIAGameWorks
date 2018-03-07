// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_FlexFluidSurface : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_FlexFluidSurface", "Flex Fluid Surface"); }	
	virtual FColor GetTypeColor() const override { return FColor(164,211,129); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Physics; }
};