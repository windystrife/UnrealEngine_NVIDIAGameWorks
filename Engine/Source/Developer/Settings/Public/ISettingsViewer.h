// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Interface for settings tabs.
 */
class ISettingsViewer
{
public:

	/**
	 * Shows the settings tab that belongs to this viewer with the specified settings section pre-selected.
	 *
	 * @param CategoryName The name of the section's category.
	 * @param SectionName The name of the section to select.
	 */
	virtual void ShowSettings( const FName& CategoryName, const FName& SectionName ) = 0;

public:

	/** Virtual destructor. */
	virtual ~ISettingsViewer() { }
};
