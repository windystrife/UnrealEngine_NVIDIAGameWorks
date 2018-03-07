// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformHttp.h"
#elif PLATFORM_PS4
#include "PS4/PS4PlatformHttp.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneHttp.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformHttp.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformHttp.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidHttp.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformHttp.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformHttp.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformHttp.h"
#endif
