//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononReverbSettingsFactory.h"
#include "PhononReverbSourceSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

namespace SteamAudio
{
	FText FAssetTypeActions_PhononReverbSettings::GetName() const
	{
		return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PhononReverbSettings", "Phonon Source Reverb Settings");
	}

	FColor FAssetTypeActions_PhononReverbSettings::GetTypeColor() const
	{
		return FColor(245, 195, 101);
	}

	UClass* FAssetTypeActions_PhononReverbSettings::GetSupportedClass() const
	{
		return UPhononReverbSourceSettings::StaticClass();
	}
	
	uint32 FAssetTypeActions_PhononReverbSettings::GetCategories()
	{
		return EAssetTypeCategories::Sounds;
	}
}

UPhononReverbSettingsFactory::UPhononReverbSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPhononReverbSourceSettings::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UPhononReverbSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
	FFeedbackContext* Warn)
{
	return NewObject<UPhononReverbSourceSettings>(InParent, InName, Flags);
}

uint32 UPhononReverbSettingsFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}