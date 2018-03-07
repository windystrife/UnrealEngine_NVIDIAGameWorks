// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_BlendSpace.h"
#include "Animation/AimOffsetBlendSpace.h"

class FAssetTypeActions_AimOffset : public FAssetTypeActions_BlendSpace
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AimOffset", "Aim Offset"); }
	virtual FColor GetTypeColor() const override { return FColor(0,162,232); }
	virtual UClass* GetSupportedClass() const override { return UAimOffsetBlendSpace::StaticClass(); }
};
