// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "Animation/AnimComposite.h"

class FAssetTypeActions_AnimComposite : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimComposite", "Animation Composite"); }
	virtual FColor GetTypeColor() const override { return FColor(181,230,29); }
	virtual UClass* GetSupportedClass() const override { return UAnimComposite::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
