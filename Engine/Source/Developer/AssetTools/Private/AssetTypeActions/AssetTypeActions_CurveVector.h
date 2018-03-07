// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Curve.h"
#include "Curves/CurveVector.h"

class FAssetTypeActions_CurveVector : public FAssetTypeActions_Curve
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CurveVector", "Vector Curve"); }
	virtual UClass* GetSupportedClass() const override { return UCurveVector::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
