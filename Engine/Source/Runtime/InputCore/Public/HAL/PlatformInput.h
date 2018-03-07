// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformInput.h"
#elif PLATFORM_PS4
#include "PS4/PS4PlatformInput.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOnePlatformInput.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformInput.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformInput.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidPlatformInput.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformInput.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformInput.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformInput.h"
#endif
