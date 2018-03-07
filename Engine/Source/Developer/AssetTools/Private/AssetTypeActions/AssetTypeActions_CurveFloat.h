// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Curve.h"
#include "Curves/CurveFloat.h"

class FAssetTypeActions_CurveFloat : public FAssetTypeActions_Curve
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CurveFloat", "Float Curve"); }
	virtual UClass* GetSupportedClass() const override { return UCurveFloat::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
