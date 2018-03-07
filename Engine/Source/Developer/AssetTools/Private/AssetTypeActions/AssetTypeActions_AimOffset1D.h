// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace1D.h"

class FAssetTypeActions_AimOffset1D : public FAssetTypeActions_BlendSpace1D
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AimOffset1D", "Aim Offset 1D"); }
	virtual FColor GetTypeColor() const override { return FColor(0,162,232); }
	virtual UClass* GetSupportedClass() const override { return UAimOffsetBlendSpace1D::StaticClass(); }
};
