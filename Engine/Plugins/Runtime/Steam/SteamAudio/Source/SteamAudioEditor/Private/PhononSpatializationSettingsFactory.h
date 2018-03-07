//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "AssetTypeActions_Base.h"
#include "PhononSpatializationSettingsFactory.generated.h"

namespace SteamAudio
{
	class FAssetTypeActions_PhononSpatializationSettings : public FAssetTypeActions_Base
	{
	public:
		virtual FText GetName() const override;
		virtual FColor GetTypeColor() const override;
		virtual UClass* GetSupportedClass() const override;
		virtual uint32 GetCategories() override;
	};
}

UCLASS(MinimalAPI, hidecategories = Object)
class UPhononSpatializationSettingsFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
		FFeedbackContext* Warn) override;

	virtual uint32 GetMenuCategories() const override;
};
