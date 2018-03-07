// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsCriticalSection.h"
#elif PLATFORM_PS4
	#include "PS4/PS4CriticalSection.h"
#elif PLATFORM_XBOXONE
	#include "XboxOne/XboxOneCriticalSection.h"
#elif PLATFORM_MAC
	#include "Mac/MacCriticalSection.h"
#elif PLATFORM_IOS
	#include "IOS/IOSCriticalSection.h"
#elif PLATFORM_ANDROID
	#include "Android/AndroidCriticalSection.h"
#elif PLATFORM_HTML5
	#include "HTML5/HTML5CriticalSection.h"
#elif PLATFORM_LINUX
	#include "Linux/LinuxCriticalSection.h"
#elif PLATFORM_SWITCH
	#include "Switch/SwitchCriticalSection.h"
#endif
