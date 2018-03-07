// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
* An interface to allow Slate Widgets to expose scrollable functionality.
*/
class SLATE_API IScrollableWidget
{
public:
	virtual ~IScrollableWidget() {}

	/** Gets the distance that user has scrolled into the control in normalized coordinates (0 - 1) */
	virtual FVector2D GetScrollDistance() = 0;

	/** Gets the distance that user has left to scroll in the control before reaching the end in normalized coordinates (0 - 1) */
	virtual FVector2D GetScrollDistanceRemaining() = 0;

	/** Returns the typed SWidget that implements this interface */
	virtual TSharedRef<class SWidget> GetScrollWidget() = 0;
};
