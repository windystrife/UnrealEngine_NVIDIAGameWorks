// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LauncherPlatformModule.h"
#if PLATFORM_WINDOWS
#include "Windows/LauncherPlatformWindows.h"
#elif PLATFORM_MAC
#include "Mac/LauncherPlatformMac.h"
#elif PLATFORM_LINUX
#include "Linux/LauncherPlatformLinux.h"
#endif

IMPLEMENT_MODULE( FLauncherPlatformModule, LauncherPlatform );
DEFINE_LOG_CATEGORY(LogLauncherPlatform);

void FLauncherPlatformModule::StartupModule()
{
	LauncherPlatform = new FLauncherPlatform();
}

void FLauncherPlatformModule::ShutdownModule()
{
	if (LauncherPlatform != NULL)
	{
		delete LauncherPlatform;
		LauncherPlatform = NULL;
	}
}