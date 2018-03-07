// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "VectorField/VectorField.h"

class FAssetTypeActions_VectorField : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_VectorField", "Vector Field"); }
	virtual FColor GetTypeColor() const override { return FColor(200,128,128); }
	virtual UClass* GetSupportedClass() const override { return UVectorField::StaticClass(); }
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};
