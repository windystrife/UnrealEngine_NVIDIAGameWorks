// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformProperties.h"
typedef FWindowsPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;
#elif PLATFORM_PS4
#include "PS4/PS4Properties.h"
typedef FPS4PlatformProperties FPlatformProperties;
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneProperties.h"
typedef FXboxOnePlatformProperties FPlatformProperties;
#elif PLATFORM_MAC
#include "Mac/MacPlatformProperties.h"
typedef FMacPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;
#elif PLATFORM_IOS
#include "IOS/IOSPlatformProperties.h"
typedef FIOSPlatformProperties FPlatformProperties;
#elif PLATFORM_ANDROID
#include "Android/AndroidProperties.h"
typedef FAndroidPlatformProperties FPlatformProperties;
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformProperties.h"
typedef FHTML5PlatformProperties FPlatformProperties;
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformProperties.h"
typedef FLinuxPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformProperties.h"
typedef FSwitchPlatformProperties FPlatformProperties;
#endif
