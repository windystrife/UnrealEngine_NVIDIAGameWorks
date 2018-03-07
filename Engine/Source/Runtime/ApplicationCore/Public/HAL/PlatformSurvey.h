// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformSurvey.h"
#elif PLATFORM_PS4
#include "PS4/PS4Survey.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneSurvey.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformSurvey.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformSurvey.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidSurvey.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformSurvey.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformSurvey.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformSurvey.h"
#endif
