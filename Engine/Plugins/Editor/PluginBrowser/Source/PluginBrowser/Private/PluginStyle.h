// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

class FPluginStyle
{
public:

	static void Initialize();
	static void Shutdown();

	static TSharedPtr< class ISlateStyle > Get();

private:

	static FString InContent( const FString& RelativePath, const ANSICHAR* Extension );

private:

	static TSharedPtr< class FSlateStyleSet > StyleSet;
};
