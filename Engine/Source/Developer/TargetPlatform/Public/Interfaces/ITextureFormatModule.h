// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class ITextureFormat;

/**
 * Interface for texture format modules.
 */
class ITextureFormatModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the texture format.
	 *
	 * @return The texture format interface.
	 */
	virtual ITextureFormat* GetTextureFormat() = 0;

public:

	/** Virtual destructor. */
	~ITextureFormatModule() { }
};
