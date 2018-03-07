// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformSplash.h"
#elif PLATFORM_PS4
#include "PS4/PS4Splash.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneSplash.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformSplash.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformSplash.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidSplash.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformSplash.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformSplash.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformSplash.h"
#endif
