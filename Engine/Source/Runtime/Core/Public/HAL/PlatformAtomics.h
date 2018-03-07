// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformAtomics.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformAtomics.h"
#elif PLATFORM_PS4
#include "PS4/PS4Atomics.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneAtomics.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformAtomics.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformAtomics.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidAtomics.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformAtomics.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformAtomics.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformAtomics.h"
#endif
