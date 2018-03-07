// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundVisualizationsPlugin.h"




class FSoundVisualizationsPlugin : public ISoundVisualizationsPlugin
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface implementation
};

IMPLEMENT_MODULE( FSoundVisualizationsPlugin, SoundVisualizations )



void FSoundVisualizationsPlugin::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FSoundVisualizationsPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



