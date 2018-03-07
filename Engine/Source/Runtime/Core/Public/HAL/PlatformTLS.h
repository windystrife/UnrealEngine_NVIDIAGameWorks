// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformTLS.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformTLS.h"
#elif PLATFORM_PS4
#include "PS4/PS4TLS.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneTLS.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformTLS.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformTLS.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidTLS.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformTLS.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformTLS.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformTLS.h"
#endif
