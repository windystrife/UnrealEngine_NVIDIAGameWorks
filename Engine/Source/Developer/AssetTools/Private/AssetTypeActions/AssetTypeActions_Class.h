// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_ClassTypeBase.h"

struct FAssetData;
class FMenuBuilder;
class IClassTypeActions;

class FAssetTypeActions_Class : public FAssetTypeActions_ClassTypeBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Class", "C++ Class"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 255, 255); }
	virtual UClass* GetSupportedClass() const override { return UClass::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic; }
	
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;

	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;

	// FAssetTypeActions_ClassTypeBase Implementation
	virtual TWeakPtr<IClassTypeActions> GetClassTypeActions(const FAssetData& AssetData) const override;
};
