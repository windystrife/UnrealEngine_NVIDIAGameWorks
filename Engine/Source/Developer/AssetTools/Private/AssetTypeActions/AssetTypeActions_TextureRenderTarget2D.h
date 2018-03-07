// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTarget.h"
#include "Engine/TextureRenderTarget2D.h"

class FAssetTypeActions_TextureRenderTarget2D : public FAssetTypeActions_TextureRenderTarget
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureRenderTarget2D", "Render Target"); }
	virtual UClass* GetSupportedClass() const override { return UTextureRenderTarget2D::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
