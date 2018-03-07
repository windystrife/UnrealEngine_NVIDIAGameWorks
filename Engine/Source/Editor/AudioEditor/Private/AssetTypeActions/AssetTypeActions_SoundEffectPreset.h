// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Sound/SoundEffectPreset.h"
#include "Widgets/SWidget.h"
#include "Developer/AssetTools/Public/AssetTypeCategories.h"
#include "Developer/AssetTools/Public/IAssetTypeActions.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundEffectSource.h"

class USoundEffectPreset;


class FAssetTypeActions_SoundEffectSubmixPreset : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSubmixPreset", "Submix Effect preset"); }
	virtual FColor GetTypeColor() const override { return FColor(175, 255, 0); }
	virtual UClass* GetSupportedClass() const override { return USoundEffectSubmixPreset::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundEffectSourcePreset : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSourcePreset", "Source Effect Preset"); }
	virtual FColor GetTypeColor() const override { return FColor(175, 255, 0); }
	virtual UClass* GetSupportedClass() const override { return USoundEffectSourcePreset::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundEffectSourcePresetChain : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSourcePresetChain", "Source Effect Preset Chain"); }
	virtual FColor GetTypeColor() const override { return FColor(175, 200, 100); }
	virtual UClass* GetSupportedClass() const override { return USoundEffectSourcePresetChain::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};



class FAssetTypeActions_SoundEffectPreset : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_SoundEffectPreset(USoundEffectPreset* InEffectPreset);

	//~ Begin FAssetTypeActions_Base
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override { return EffectPreset->GetPresetColor(); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	//~ End FAssetTypeActions_Base

private:

	USoundEffectPreset* EffectPreset;
};