// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Materials/MaterialParameterCollection.h"

class FAssetTypeActions_MaterialParameterCollection : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MaterialParameterCollection", "Material Parameter Collection"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 192, 0); }
	virtual UClass* GetSupportedClass() const override { return UMaterialParameterCollection::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
};
