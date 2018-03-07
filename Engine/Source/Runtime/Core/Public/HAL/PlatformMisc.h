// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMisc.h"
#elif PLATFORM_PS4
#include "PS4/PS4Misc.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneMisc.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformMisc.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformMisc.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidMisc.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformMisc.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformMisc.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformMisc.h"
#endif

class CORE_API FScopedNamedEvent
{
public:

	FScopedNamedEvent(const struct FColor& Color, const TCHAR* Text)
	{
		FPlatformMisc::BeginNamedEvent(Color, Text);
	}

	FScopedNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
	{
		FPlatformMisc::BeginNamedEvent(Color, Text);
	}

	~FScopedNamedEvent()
	{
		FPlatformMisc::EndNamedEvent();
	}	
};

//lightweight scoped named event separate from stats system.  Will be available in test builds.  
//Events cost profiling overhead so use them judiciously in final code.
#if !UE_BUILD_SHIPPING
#define SCOPED_NAMED_EVENT(Name, Color) FScopedNamedEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__)(Color, #Name);
#else
#define SCOPED_NAMED_EVENT(Name, Color)
#endif
