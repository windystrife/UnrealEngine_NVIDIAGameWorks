// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformStackWalk.h"
#elif PLATFORM_PS4
#include "PS4/PS4StackWalk.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneStackWalk.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformStackWalk.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformStackWalk.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidPlatformStackWalk.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformStackWalk.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformStackWalk.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformStackWalk.h"
#endif
