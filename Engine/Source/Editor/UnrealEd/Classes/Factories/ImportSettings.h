// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Base class for all factories to provide import settings
 * 
 * Supports filling the settings from a json file for automated importing
 */
class UNREALED_API IImportSettingsParser
{
public:
	/**
	 * Parses settings data from a json object.  The format of the object is dependent on what the derived types expect
	 */
	virtual void ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson) = 0;
};



