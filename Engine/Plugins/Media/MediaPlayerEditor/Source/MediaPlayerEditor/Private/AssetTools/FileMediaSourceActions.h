// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Styling/ISlateStyle.h"
#include "AssetTools/MediaSourceActions.h"

struct FAssetData;

/**
 * Implements an action for UMediaSource assets.
 */
class FFileMediaSourceActions
	: public FMediaSourceActions
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use for asset editor toolkits.
	 */
	FFileMediaSourceActions(const TSharedRef<ISlateStyle>& InStyle);

public:

	//~ FAssetTypeActions_Base interface

	virtual bool CanFilter() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;

private:

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;
};
