// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LauncherPlatformLinux.h"

bool FLauncherPlatformLinux::CanOpenLauncher(bool Install)
{
	// TODO: no launcher support at the moment
	return false;
}

bool FLauncherPlatformLinux::OpenLauncher(const FOpenLauncherOptions& Options)
{
	// TODO: support launcher for realz
	return true;
}
