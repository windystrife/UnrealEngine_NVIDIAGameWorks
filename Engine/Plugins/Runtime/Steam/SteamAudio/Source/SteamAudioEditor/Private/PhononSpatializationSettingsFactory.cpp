//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSpatializationSettingsFactory.h"
#include "PhononSpatializationSourceSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

namespace SteamAudio
{
	FText FAssetTypeActions_PhononSpatializationSettings::GetName() const
	{
		return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PhononSpatializationSettings", "Phonon Source Spatialization Settings");
	}

	FColor FAssetTypeActions_PhononSpatializationSettings::GetTypeColor() const
	{
		return FColor(245, 195, 101);
	}

	UClass* FAssetTypeActions_PhononSpatializationSettings::GetSupportedClass() const
	{
		return UPhononSpatializationSourceSettings::StaticClass();
	}

	uint32 FAssetTypeActions_PhononSpatializationSettings::GetCategories()
	{
		return EAssetTypeCategories::Sounds;
	}
}

UPhononSpatializationSettingsFactory::UPhononSpatializationSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPhononSpatializationSourceSettings::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UPhononSpatializationSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPhononSpatializationSourceSettings>(InParent, InName, Flags);
}

uint32 UPhononSpatializationSettingsFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}