// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILauncherPlatform.h"

class FLauncherPlatformWindows : public ILauncherPlatform
{
public:
	// ILauncherPlatform Implementation
	virtual bool CanOpenLauncher(bool Install) override;
	virtual bool OpenLauncher(const FOpenLauncherOptions& Options) override;

private:
	bool IsLauncherInstalled() const;
	bool GetLauncherInstallerPath(FString& OutInstallerPath);
};

typedef FLauncherPlatformWindows FLauncherPlatform;
