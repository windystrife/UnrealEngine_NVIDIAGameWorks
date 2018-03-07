// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"

class FMenuBuilder;

class FAssetTypeActions_MaterialInstanceConstant : public FAssetTypeActions_MaterialInterface
{
public:
	FAssetTypeActions_MaterialInstanceConstant(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{
	}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MaterialInstanceConstant", "Material Instance"); }
	virtual FColor GetTypeColor() const override { return FColor(0,128,0); }
	virtual UClass* GetSupportedClass() const override { return UMaterialInstanceConstant::StaticClass(); }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override { return FAssetTypeActions_MaterialInterface::GetCategories() | AssetCategoryBit; }

private:
	/** Handler for when FindParent is selected */
	void ExecuteFindParent(TArray<TWeakObjectPtr<UMaterialInstanceConstant>> Objects);

	EAssetTypeCategories::Type AssetCategoryBit;
};
