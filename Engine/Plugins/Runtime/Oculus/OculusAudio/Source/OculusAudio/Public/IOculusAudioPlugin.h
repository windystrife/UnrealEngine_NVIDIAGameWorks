// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "OculusAudio.h"
#include "OculusAudioDllManager.h"

/************************************************************************/
/* FOculusAudioPlugin                                                   */
/* Module interface. Also handles loading and unloading the Oculus Audio*/
/* DLL using FOculusAudioDllManager.                                    */
/************************************************************************/
class FOculusAudioPlugin : public IModuleInterface
{
public:
	//~ Begin IModuleInterface 
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	FOculusSpatializationPluginFactory PluginFactory;
};
