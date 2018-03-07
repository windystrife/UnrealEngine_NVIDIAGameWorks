// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformOutputDevices.h"
#elif PLATFORM_PS4
#include "PS4/PS4OutputDevices.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneOutputDevices.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformOutputDevices.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformOutputDevices.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidOutputDevices.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformOutputDevices.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformOutputDevices.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformOutputDevices.h"
#endif
