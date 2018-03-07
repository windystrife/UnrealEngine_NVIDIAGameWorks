// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformFile.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformFile.h"
#elif PLATFORM_PS4
#include "PS4/PS4File.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneFile.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformFile.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformFile.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidFile.h"
#elif PLATFORM_HTML5
//#include "HTML5PlatformFile.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformFile.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformFile.h"
#endif
