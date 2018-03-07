// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
* Implements the visual style of LogVisualizer.
*/
class FLogVisualizerStyle
{
public:
	static void Initialize();

	static void Shutdown();

	/** @return The Slate style set for Fortnite Editor */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create();

private:

	static TSharedPtr< class FSlateStyleSet > StyleInstance;
};

