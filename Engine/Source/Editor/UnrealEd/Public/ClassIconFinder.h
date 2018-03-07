// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"
#include "Styling/SlateIconFinder.h"

class AActor;
struct FAssetData;
class ISlateStyle;
class UBlueprint;
struct FSlateBrush;

class FClassIconFinder
{
public:

	/** Find the best fitting small icon to use for the supplied actor array */
	UNREALED_API static const FSlateBrush* FindIconForActors(const TArray< TWeakObjectPtr<AActor> >& InActors, UClass*& CommonBaseClass);

	/** Find the small icon to use for the supplied actor */
	UNREALED_API static const FSlateBrush* FindIconForActor( const TWeakObjectPtr<AActor>& InActor );
	
	/** Find the small icon to use for the supplied actor */
	UNREALED_API static FSlateIcon FindSlateIconForActor( const TWeakObjectPtr<AActor>& InActor );

	/** Utility function to convert a Blueprint into the most suitable class possible for use by the icon finder */
	UNREALED_API static const UClass* GetIconClassForBlueprint(const UBlueprint* InBlueprint);

	/** Utility function to convert an asset into the most suitable class possible for use by the icon finder */
	UNREALED_API static const UClass* GetIconClassForAssetData(const FAssetData& InAssetData, bool* bOutIsClassType = nullptr);

	/** Find the large thumbnail name to use for the supplied class */
	UNREALED_API static const FSlateBrush* FindThumbnailForClass(const UClass* InClass, const FName& InDefaultName = FName() )
	{
		return FSlateIconFinder::FindCustomIconBrushForClass(InClass, TEXT("ClassThumbnail"), InDefaultName);
	}

public:

	/** Registers a new style set to use to try to find icons. */
	DEPRECATED(4.13, "This function is no longer required.")
	static void RegisterIconSource(const ISlateStyle* StyleSet){}

	/** Unregisters an style set. */
	DEPRECATED(4.13, "This function is no longer required.")
	static void UnregisterIconSource(const ISlateStyle* StyleSet){}

	/** Find the small icon to use for the supplied class */
	DEPRECATED(4.13, "Please use FSlateIconFinder::FindIconForClass directly (followed by FSlateIcon::GetIcon().")
	static const FSlateBrush* FindIconForClass(const UClass* InClass, const FName& InDefaultName = FName() )
	{
		return FSlateIconFinder::FindIconBrushForClass(InClass, InDefaultName);
	}

	/** Find the small icon name to use for the supplied class */
	DEPRECATED(4.13, "Please use FSlateIconFinder::FindIconForClass directly (followed by FSlateIcon::GetStyleName().")
	static FName FindIconNameForClass(const UClass* InClass, const FName& InDefaultName = FName() )
	{
		return FSlateIconFinder::FindIconForClass(InClass, InDefaultName).GetStyleName();
	}

	/** Find the large thumbnail name to use for the supplied class */
	DEPRECATED(4.13, "Please use FSlateIconFinder::FindCustomIconForClass directly (followed by FSlateIcon::GetStyleName().")
	static FName FindThumbnailNameForClass(const UClass* InClass, const FName& InDefaultName = FName() )
	{
		return FSlateIconFinder::FindCustomIconForClass(InClass, TEXT("ClassThumbnail"), InDefaultName).GetStyleName();
	}

	/** Find the small icon name to use for the supplied actor */
	DEPRECATED(4.13, "Please use FindSlateIconForActor directly.")
	static FName FindIconNameForActor( const TWeakObjectPtr<AActor>& InActor )
	{
		return FindSlateIconForActor(InActor).GetStyleName();
	}

private:
	UNREALED_API static const FSlateBrush* LookupBrush(FName IconName);
};
