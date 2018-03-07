// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "Engine/UserDefinedEnum.h"

class FAssetTypeActions_Enum : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Enum", "Enumeration"); }
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual FColor GetTypeColor() const override { return FColor(255, 200, 200); }
	virtual UClass* GetSupportedClass() const override { return UUserDefinedEnum::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Blueprint; }
	virtual bool CanLocalize() const override { return false; }

	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
};
