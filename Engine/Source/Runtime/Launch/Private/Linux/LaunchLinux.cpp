// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LaunchEngineLoop.h"
#include "LinuxCommonStartup.h"

extern int32 GuardedMain( const TCHAR* CmdLine );

/**
 * Workaround function to avoid circular dependencies between Launch and CommonLinuxStartup modules.
 *
 * Other platforms call FEngineLoop::AppExit() in their main() (removed by preprocessor if compiled without engine), but on Linux we want to share a common main() in CommonLinuxStartup module,
 * so not just the engine but all the programs could share this logic. Unfortunately, AppExit() practice breaks this nice approach since FEngineLoop cannot be moved outside of Launch without
 * making too many changes. Hence CommonLinuxMain will call it through this function if WITH_ENGINE is defined.
 *
 * If you change the prototype here, update CommonLinuxMain() too!
 */
void LAUNCH_API LaunchLinux_FEngineLoop_AppExit()
{
	return FEngineLoop::AppExit();
}

int main(int argc, char *argv[])
{
	return CommonLinuxMain(argc, argv, &GuardedMain);
}
