// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMemory.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMemory.h"
#elif PLATFORM_PS4

#if USE_NEW_PS4_MEMORY_SYSTEM
#include "PS4/PS4Memory2.h"
#else
#include "PS4/PS4Memory.h"
#endif

#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneMemory.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformMemory.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformMemory.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidMemory.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformMemory.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformMemory.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformMemory.h"
#endif
