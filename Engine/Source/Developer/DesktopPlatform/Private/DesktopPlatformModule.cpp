// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DesktopPlatformModule.h"
#include "DesktopPlatformPrivate.h"

IMPLEMENT_MODULE( FDesktopPlatformModule, DesktopPlatform );
DEFINE_LOG_CATEGORY(LogDesktopPlatform);

void FDesktopPlatformModule::StartupModule()
{
	DesktopPlatform = new FDesktopPlatform();
}

void FDesktopPlatformModule::ShutdownModule()
{
	if (DesktopPlatform != NULL)
	{
		delete DesktopPlatform;
		DesktopPlatform = NULL;
	}
}
