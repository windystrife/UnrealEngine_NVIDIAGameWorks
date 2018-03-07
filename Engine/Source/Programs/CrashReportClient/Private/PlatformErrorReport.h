// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

#include "Windows/WindowsErrorReport.h"

typedef FWindowsErrorReport FPlatformErrorReport;

#elif PLATFORM_LINUX

#include "Linux/LinuxErrorReport.h"

typedef FLinuxErrorReport FPlatformErrorReport;

#elif PLATFORM_MAC

#include "Mac/MacErrorReport.h"

typedef FMacErrorReport FPlatformErrorReport;

#endif // PLATFORM_LINUX
