// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Like a null widget, but visualizes itself as being explicitly missing.
 */
class SLATE_API SMissingWidget
{
public:

	/**
	 * Creates a new instance.
	 *
	 * @return The widget.
	 */
	static TSharedRef<class SWidget> MakeMissingWidget( );
};
