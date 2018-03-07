// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Styling/SlateIconFinder.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

FSlateIcon FSlateIconFinder::FindIconForClass(const UClass* InClass, const FName& InDefaultName)
{
	return FindCustomIconForClass(InClass, TEXT("ClassIcon"), InDefaultName);
}

FSlateIcon FSlateIconFinder::FindCustomIconForClass(const UClass* InClass, const TCHAR* StyleBasePath, const FName& InDefaultName)
{
	FSlateIcon Icon;

	FString IconPath(StyleBasePath);
	IconPath += TEXT(".");
	const int32 BastPathLength = IconPath.Len();

	if (InClass)
	{
		// walk up class hierarchy until we find an icon
		const UClass* CurrentClass = InClass;
		while ( CurrentClass)
		{
			CurrentClass->AppendName(IconPath);
			Icon = FindIcon(*IconPath);

			if (Icon.IsSet())
			{
				return Icon;
			}

			CurrentClass = CurrentClass->GetSuperClass();
			IconPath.RemoveAt(BastPathLength, IconPath.Len() - BastPathLength, false);
		}
	}

	// If we didn't supply an override name for the default icon use default class icon.
	if (InDefaultName.IsNone())
	{
		IconPath += TEXT("Default");
		return FindIcon(*IconPath);
	}

	return FindIcon(InDefaultName);
}

FSlateIcon FSlateIconFinder::FindIcon(const FName& IconName)
{
	FSlateIcon Icon;

	FSlateStyleRegistry::IterateAllStyles(
		[&](const ISlateStyle& Style)
		{
			if (Style.GetOptionalBrush(IconName, nullptr, nullptr))
			{
				Icon = FSlateIcon(Style.GetStyleSetName(), IconName);
				// terminate iteration
				return false;
			}
			// continue iteration
			return true;
		}
	);

	return Icon;
}
