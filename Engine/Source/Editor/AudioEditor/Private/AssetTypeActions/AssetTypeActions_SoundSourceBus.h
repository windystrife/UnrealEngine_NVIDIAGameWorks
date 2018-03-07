// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Sound/SoundSourceBus.h"
#include "AssetTypeActions/AssetTypeActions_SoundBase.h"

class FMenuBuilder;
class USoundSourceBus;

class FAssetTypeActions_SoundSourceBus : public FAssetTypeActions_SoundBase
{
public:
	//~ Begin IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSourceBus", "Sound Source Bus"); }
	virtual FColor GetTypeColor() const override { return FColor(212, 97, 85); }
	virtual UClass* GetSupportedClass() const override;
	virtual bool CanFilter() override { return true; }
	virtual bool IsImportedAsset() const override { return false; }
	//~ End IAssetTypeActions Implementation
};
