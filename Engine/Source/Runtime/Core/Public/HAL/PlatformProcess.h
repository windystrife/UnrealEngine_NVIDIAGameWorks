// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformProcess.h"
#elif PLATFORM_PS4
#include "PS4/PS4Process.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneProcess.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformProcess.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformProcess.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidProcess.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformProcess.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformProcess.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformProcess.h"
#endif
