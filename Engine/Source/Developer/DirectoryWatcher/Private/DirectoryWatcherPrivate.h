// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDirectoryWatcher, Log, All);

#if PLATFORM_WINDOWS
#include "Windows/DirectoryWatchRequestWindows.h"
#include "Windows/DirectoryWatcherWindows.h"
#elif PLATFORM_MAC
#include "Mac/DirectoryWatchRequestMac.h"
#include "Mac/DirectoryWatcherMac.h"
#elif PLATFORM_LINUX
#include "Linux/DirectoryWatchRequestLinux.h"
#include "Linux/DirectoryWatcherLinux.h"
#else
#error "Unknown platform"
#endif
