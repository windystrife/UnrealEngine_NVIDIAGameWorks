// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Materials/MaterialInterface.h"

class FMenuBuilder;

class FAssetTypeActions_MaterialInterface : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MaterialInterface", "Material Interface"); }
	virtual FColor GetTypeColor() const override { return FColor(64,192,64); }
	virtual UClass* GetSupportedClass() const override { return UMaterialInterface::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual EThumbnailPrimType GetDefaultThumbnailPrimitiveType(UObject* Asset) const override;

private:
	/** Handler for when NewMIC is selected */
	void ExecuteNewMIC(TArray<TWeakObjectPtr<UMaterialInterface>> Objects);
};
