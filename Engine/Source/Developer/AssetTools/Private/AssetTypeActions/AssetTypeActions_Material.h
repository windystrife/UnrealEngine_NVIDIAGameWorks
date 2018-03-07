// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_MaterialInterface.h"
#include "Materials/Material.h"

class FAssetTypeActions_Material : public FAssetTypeActions_MaterialInterface
{
public:
	FAssetTypeActions_Material(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{
	}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Material", "Material"); }
	virtual FColor GetTypeColor() const override { return FColor(64,192,64); }
	virtual UClass* GetSupportedClass() const override { return UMaterial::StaticClass(); }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override { return FAssetTypeActions_MaterialInterface::GetCategories() | EAssetTypeCategories::Basic | AssetCategoryBit; }

private:

	EAssetTypeCategories::Type AssetCategoryBit;
};
