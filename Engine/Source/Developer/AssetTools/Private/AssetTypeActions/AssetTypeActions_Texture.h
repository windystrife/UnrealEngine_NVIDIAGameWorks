// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;

class FAssetTypeActions_Texture : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Texture", "BaseTexture"); }
	virtual FColor GetTypeColor() const override { return FColor(192,64,64); }
	virtual UClass* GetSupportedClass() const override { return UTexture::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

private:
	/** Handler for when CreateMaterial is selected */
	void ExecuteCreateMaterial(TArray<TWeakObjectPtr<UTexture>> Objects);

	/** Handler for when CreateSubUVAnimation is selected */
	void ExecuteCreateSubUVAnimation(TArray<TWeakObjectPtr<UTexture>> Objects);

	/** Handler for when FindMaterials is selected */
	void ExecuteFindMaterials(TWeakObjectPtr<UTexture> Object);
};
