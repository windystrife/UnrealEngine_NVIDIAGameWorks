// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformString.h"
#elif PLATFORM_PS4
#include "PS4/PS4String.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneString.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformString.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformString.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidString.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformString.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformString.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformString.h"
#endif

