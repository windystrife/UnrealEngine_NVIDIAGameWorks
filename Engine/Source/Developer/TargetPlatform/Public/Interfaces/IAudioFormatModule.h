// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IAudioFormat;

/**
 * Interface for audio format modules.
 */
class IAudioFormatModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the audio format.
	 *
	 * @return The audio format interface.
	 */
	virtual IAudioFormat* GetAudioFormat() = 0;

public:

	/** Virtual destructor. */
	~IAudioFormatModule() { }
};
