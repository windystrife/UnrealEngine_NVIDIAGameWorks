// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "AssetTypeActions_Base.h"

struct FAssetData;

/**
 * Implements an action for UMediaSource assets.
 */
class FMediaSourceActions
	: public FAssetTypeActions_Base
{
public:

	//~ FAssetTypeActions_Base interface

	virtual bool CanFilter() override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
};
