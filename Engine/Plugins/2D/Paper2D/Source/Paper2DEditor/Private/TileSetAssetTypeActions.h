// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;
class UPaperTileSet;

class FTileSetAssetTypeActions : public FAssetTypeActions_Base
{
public:
	FTileSetAssetTypeActions(EAssetTypeCategories::Type InAssetCategory);

	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	// End of IAssetTypeActions interface

private:
	EAssetTypeCategories::Type MyAssetCategory;

private:
	void ExecuteCreateTileMap(TWeakObjectPtr<UPaperTileSet> TileSetPtr);
	void ExecutePadTileSetTexture(TWeakObjectPtr<UPaperTileSet> TileSetPtr);
};
