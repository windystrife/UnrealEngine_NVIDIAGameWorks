// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"

struct FSlateBrush;

/** Class used for finding icons within a registered set of styles */
class SLATECORE_API FSlateIconFinder
{
public:

	/**
	 * Find the icon to use for the supplied class
	 *
	 * @param InClass			The class to locate an icon for
	 * @param InDefaultName		The default icon name to use, if no specialized icon could be found
	 * @return An FSlateIcon structure defining the class's icon
	 */
	static FSlateIcon FindIconForClass(const UClass* InClass, const FName& InDefaultName = FName());

	/**
	 * Find a custom icon to use for the supplied class, according to the specified base style
	 *
	 * @param InClass			The class to locate an icon for
	 * @param InStyleBasePath	Style base path to use for the search (e.g. ClassIcon, or ClassThumbnail)
	 * @param InDefaultName		The default icon name to use, if no specialized icon could be found
	 * @return An FSlateIcon structure defining the class's icon
	 */
	static FSlateIcon FindCustomIconForClass(const UClass* InClass, const TCHAR* StyleBasePath, const FName& InDefaultName = FName());

	/**
	 * Find a slate brush to use for the supplied class's icon
	 *
	 * @param InClass			The class to locate an icon for
	 * @param InDefaultName		The default icon name to use, if no specialized icon could be found
	 * @return A slate brush, or nullptr if no icon was found
	 */
	static const FSlateBrush* FindIconBrushForClass(const UClass* InClass, const FName& InDefaultName = FName())
	{
		return FindIconForClass(InClass, InDefaultName).GetOptionalIcon();
	}
	
	/**
	 * Find a custom icon to use for the supplied class, according to the specified base style
	 *
	 * @param InClass			The class to locate an icon for
	 * @param InStyleBasePath	Style base path to use for the search (e.g. ClassIcon, or ClassThumbnail)
	 * @param InDefaultName		The default icon name to use, if no specialized icon could be found
	 * @return A slate brush, or nullptr if no icon was found
	 */
	static const FSlateBrush* FindCustomIconBrushForClass(const UClass* InClass, const TCHAR* StyleBasePath, const FName& InDefaultName = FName())
	{
		return FindCustomIconForClass(InClass, StyleBasePath, InDefaultName).GetOptionalIcon();
	}

	/**
	 * Find the first occurance of a brush represented by the specified IconName in any of the registered style sets
	 * @param InIconName 		The fully qualified style name of the icon to find
	 * @return An FSlateIcon structure
	 */
	static FSlateIcon FindIcon(const FName& InIconName);
};
