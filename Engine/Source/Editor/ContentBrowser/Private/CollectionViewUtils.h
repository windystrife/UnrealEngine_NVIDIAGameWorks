// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollectionManagerTypes.h"

namespace CollectionViewUtils
{
	/**
	 * Loads the color of this collection from the config
	 *
	 * @param InCollectionName - The name of the collection to get the color for
	 * @param InCollectionType - The type of the collection to get the color for
	 * @return The color the collection should appear as, will be null if not customized
	 */
	const TSharedPtr<FLinearColor> LoadColor(const FString& InCollectionName, const ECollectionShareType::Type& InCollectionType);

	/**
	 * Saves the color of the collection to the config
	 *
	 * @param InCollectionName - The name of the collection to set the color for
	 * @param InCollectionType - The type of the collection to set the color for
	 * @param CollectionColor - The color the collection should appear as
	 * @param bForceAdd - If true, force the color to be added even if it's the default color
	 */
	void SaveColor(const FString& InCollectionName, const ECollectionShareType::Type& InCollectionType, const TSharedPtr<FLinearColor> CollectionColor, const bool bForceAdd = false);

	/**
	 * Checks to see if any collection has a custom color, optionally outputs them to a list
	 *
	 * @param OutColors - If specified, returns all the custom colors being used
	 * @return true, if custom colors are present
	 */
	bool HasCustomColors( TArray< FLinearColor >* OutColors = nullptr );

	/** Gets the default color the collection should appear as */
	FLinearColor GetDefaultColor();
}
