// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformApplicationMisc.h"
#elif PLATFORM_PS4
#include "PS4/PS4ApplicationMisc.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneApplicationMisc.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformApplicationMisc.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformApplicationMisc.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidApplicationMisc.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformApplicationMisc.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformApplicationMisc.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformApplicationMisc.h"
#endif
