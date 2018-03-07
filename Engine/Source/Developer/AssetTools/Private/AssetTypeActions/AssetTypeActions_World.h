// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_World : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_World", "Level"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 156, 0); }
	virtual UClass* GetSupportedClass() const override { return UWorld::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return false; }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic; }
	virtual bool CanLocalize() const override { return false; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
};
