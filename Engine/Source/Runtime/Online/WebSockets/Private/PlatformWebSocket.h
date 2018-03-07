// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX || PLATFORM_PS4
#include "Lws/LwsWebSocketsManager.h"
typedef FLwsWebSocketsManager FPlatformWebSocketsManager;
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneWebSocketsManager.h"
typedef FXboxOneWebSocketsManager FPlatformWebSocketsManager;
#else
#error "Web sockets not implemented on this platform yet"
#endif
