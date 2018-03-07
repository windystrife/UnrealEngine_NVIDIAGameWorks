// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Styling/SlateStyle.h"

class ISlateStyle;

class FGameMenuBuilderStyle
{
public:

	static void Initialize(const FString StyleName);

	static void Shutdown();
	
	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set  */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();
		
	GAMEMENUBUILDER_API static FString FontName;
	GAMEMENUBUILDER_API static int32	FontSize;
private:

	static TSharedRef< class FSlateStyleSet > Create(const FString StyleName);

	static TSharedPtr< class FSlateStyleSet > SimpleStyleInstance;
};

