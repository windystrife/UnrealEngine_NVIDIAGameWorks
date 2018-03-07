// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioExtensionPlugin.h"
#include "OculusAudioMixer.h"
#include "OculusAudioLegacy.h"

/************************************************************************/
/* FOculusSpatializationPluginFactory                                   */
/* Handles initialization of the required Oculus Audio Spatialization   */
/* plugin.                                                              */
/************************************************************************/
class FOculusSpatializationPluginFactory : public IAudioSpatializationFactory
{
	//~ Begin IAudioSpatializationFactory
	virtual FString GetDisplayName() override
	{
		static FString DisplayName = FString(TEXT("Oculus Audio"));
		return DisplayName;
	}

	virtual bool SupportsPlatform(EAudioPlatform Platform) override
	{
		if (Platform == EAudioPlatform::Windows)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	virtual TAudioSpatializationPtr CreateNewSpatializationPlugin(FAudioDevice* OwningDevice) override;
	//~ End IAudioSpatializationFactory
};
