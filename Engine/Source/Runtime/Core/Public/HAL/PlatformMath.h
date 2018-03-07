// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMath.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMath.h"
#elif PLATFORM_PS4
#include "PS4/PS4Math.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneMath.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformMath.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformMath.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidMath.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformMath.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformMath.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformMath.h"
#endif
