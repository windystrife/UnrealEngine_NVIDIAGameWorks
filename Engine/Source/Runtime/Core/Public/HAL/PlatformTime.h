// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformTime.h"
#elif PLATFORM_PS4
#include "PS4/PS4Time.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneTime.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformTime.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformTime.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidTime.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformTime.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformTime.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformTime.h"
#endif
