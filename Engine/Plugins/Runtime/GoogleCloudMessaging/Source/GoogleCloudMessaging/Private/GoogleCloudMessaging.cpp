// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GoogleCloudMessaging.h"

void IGoogleCloudMessagingModuleInterface::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void IGoogleCloudMessagingModuleInterface::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#if WITH_EDITOR

DEFINE_LOG_CATEGORY( LogGoogleCloudMessaging );

class FEditorGoogleCloudMessaging : public IGoogleCloudMessagingModuleInterface
{
};

IMPLEMENT_MODULE( FEditorGoogleCloudMessaging, GoogleCloudMessaging )

#endif

