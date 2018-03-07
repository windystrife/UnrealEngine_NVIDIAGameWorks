// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "GroundTruthData.h"

class FMenuBuilder;

class FAssetTypeActions_GroundTruthData : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FColor GetTypeColor() const override { return FColor(255, 196, 128); }
	virtual bool CanLocalize() const override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GroundTruthData", "Ground Truth Data"); }
	virtual UClass* GetSupportedClass() const override { return UGroundTruthData::StaticClass(); }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	// End IAssetTypeActions
};
