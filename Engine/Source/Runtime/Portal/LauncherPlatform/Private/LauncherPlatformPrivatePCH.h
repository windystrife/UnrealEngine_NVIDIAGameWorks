// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLauncherPlatform, Log, All);

#include "ILauncherPlatform.h"

#if PLATFORM_WINDOWS
#include "Windows/LauncherPlatformWindows.h"
#elif PLATFORM_MAC
#include "Mac/LauncherPlatformMac.h"
#elif PLATFORM_LINUX
#include "Linux/LauncherPlatformLinux.h"
#endif
