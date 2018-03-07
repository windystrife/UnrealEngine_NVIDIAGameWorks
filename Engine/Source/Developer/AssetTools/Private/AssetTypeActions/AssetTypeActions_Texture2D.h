// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"

class FMenuBuilder;

class FAssetTypeActions_Texture2D : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Texture2D", "Texture"); }
	virtual FColor GetTypeColor() const override { return FColor(192,64,64); }
	virtual UClass* GetSupportedClass() const override { return UTexture2D::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override { return FAssetTypeActions_Texture::GetCategories() | EAssetTypeCategories::Basic; }

private:
	/** Handler for when Create Slate Brush is selected */
	void ExecuteCreateSlateBrush(TArray<TWeakObjectPtr<UTexture>> Objects);
};
