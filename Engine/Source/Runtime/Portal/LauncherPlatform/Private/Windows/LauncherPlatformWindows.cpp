// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LauncherPlatformWindows.h"
#include "WindowsPlatformProcess.h"
#include "WindowsHWrapper.h"
#include "Paths.h"

bool FLauncherPlatformWindows::CanOpenLauncher(bool Install)
{
	// Check if the launcher exists, or (optionally) if the installer exists
	FString Path;
	return IsLauncherInstalled() || (Install && GetLauncherInstallerPath(Path));
}

bool FLauncherPlatformWindows::OpenLauncher(const FOpenLauncherOptions& Options)
{	
	// Try to launch it directly
	FString InstallerPath;
	if (IsLauncherInstalled())
	{
		FString LauncherUriRequest = Options.GetLauncherUriRequest();

		FString Error;
		FPlatformProcess::LaunchURL(*LauncherUriRequest, nullptr, &Error);
		return true;
	}
	// Otherwise see if we can install it
	else if(Options.bInstall && GetLauncherInstallerPath(InstallerPath))
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*InstallerPath);
		return true;
	}

	return false;
}

bool FLauncherPlatformWindows::IsLauncherInstalled() const
{
	bool bRes = false;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("com.epicgames.launcher"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		bRes = true;
	}

	return bRes;
}

bool FLauncherPlatformWindows::GetLauncherInstallerPath(FString& OutInstallerPath)
{
	FString InstallerPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EngineDir(), TEXT("Extras/UnrealEngineLauncher/EpicGamesLauncherInstaller.msi")));
	if (FPaths::FileExists(InstallerPath))
	{
		OutInstallerPath = InstallerPath;
		return true;
	}

	return false;
}
