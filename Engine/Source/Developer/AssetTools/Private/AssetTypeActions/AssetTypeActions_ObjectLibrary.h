// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Engine/ObjectLibrary.h"

class FAssetTypeActions_ObjectLibrary : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ObjectLibrary", "Object Library"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 63, 108); }
	virtual UClass* GetSupportedClass() const override { return UObjectLibrary::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};
