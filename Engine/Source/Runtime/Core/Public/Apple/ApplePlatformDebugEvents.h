// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	FApplePlatformDebugEvents.h: Apple platform debug events
==============================================================================================*/

#pragma once
#include "CoreTypes.h"
#include "LogMacros.h"
#include "Map.h"
#include "UnrealString.h"
#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#elif PLATFORM_IOS
#include "IOS/IOSSystemIncludes.h"
#endif

#if !defined(APPLE_PROFILING_ENABLED)
#define APPLE_PROFILING_ENABLED 0
#endif

#if APPLE_PROFILING_ENABLED

DECLARE_LOG_CATEGORY_EXTERN(LogInstruments, Display, All);

struct FApplePlatformDebugEvents
{
	static void DebugSignPost(uint16 Code, uintptr_t Arg1, uintptr_t Arg2, uintptr_t Arg3, uintptr_t Arg4);

	static void DebugSignPostStart(uint16 Code, uintptr_t Arg1, uintptr_t Arg2, uintptr_t Arg3, uintptr_t Arg4);

	static void DebugSignPostEnd(uint16 Code, uintptr_t Arg1, uintptr_t Arg2, uintptr_t Arg3, uintptr_t Arg4);
	
	struct FEvent
	{
		void const* Tag;
		uint32 Color;
		uint16 Code;
	};
	
	static TArray<FEvent>* GetEventStack();
	
	static uint16 GetEventCode(FString String);
	
	static void BeginNamedEvent(const struct FColor& Color,const TCHAR* Text);
	
	static void BeginNamedEvent(const struct FColor& Color,const ANSICHAR* Text);
	
	static void EndNamedEvent();
	
	static uint32 TLSSlot;
};

#endif
