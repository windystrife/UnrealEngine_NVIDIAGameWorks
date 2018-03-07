// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformDebugEvents.cpp: Apple platform implementations of File functions
=============================================================================*/

#include "ApplePlatformDebugEvents.h"
#include "ApplePlatformTLS.h"
#include "UnrealString.h"
#include "IConsoleManager.h"
#include "Color.h"
#include "ScopeRWLock.h"
#include <sys/kdebug_signpost.h>
#include <sys/syscall.h>

/*------------------------------------------------------------------------------
 Legacy OS Defines.
 ------------------------------------------------------------------------------*/

#ifndef DBG_MACH_CHUD

#define DBG_MACH_CHUD 0x0A
#define DBG_FUNC_NONE 0
#define DBG_FUNC_START 1
#define DBG_FUNC_END 2
#define DBG_APPS              33

#define KDBG_CODE(Class, SubClass, code) (((Class & 0xff) << 24) | ((SubClass & 0xff) << 16) | ((code & 0x3fff)  << 2))
#define APPSDBG_CODE(SubClass,code) KDBG_CODE(DBG_APPS, SubClass, code)

#endif

/*------------------------------------------------------------------------------
 Defines.
 ------------------------------------------------------------------------------*/

#if APPLE_PROFILING_ENABLED

#define APPLE_PROFILING_FALLBACKS !PLATFORM_TVOS && ((!PLATFORM_MAC && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0) || (PLATFORM_MAC && __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_12))

DEFINE_LOG_CATEGORY(LogInstruments)

/*------------------------------------------------------------------------------
 Console variables.
 ------------------------------------------------------------------------------*/

static int32 GAppleInstrumentsEvents = 0;
static FAutoConsoleVariableRef CVarAppleInstrumentsEvent(
	TEXT("Apple.InstrumentsEvents"),
	GAppleInstrumentsEvents,
	TEXT("Set to true (>0) to emit scoped kdebug events for Instruments, which has a noticeable performance impact or 0 to disable. (Default: 0, off)"),
	ECVF_Default);

/*------------------------------------------------------------------------------
 Implementation.
 ------------------------------------------------------------------------------*/

uint32 FApplePlatformDebugEvents::TLSSlot = FPlatformTLS::AllocTlsSlot();

enum EInstrumentsColors
{
	Blue = 0,
	Green = 1,
	Purple = 2,
	Orange = 3,
	Red = 4,
	Max
};

static uint32 GetInstrumentsColor(FColor const& Color)
{
	uint32 Diff[EInstrumentsColors::Max];
	Diff[EInstrumentsColors::Blue] = FColor::Blue.DWColor() - Color.DWColor();
	Diff[EInstrumentsColors::Green] = FColor::Green.DWColor() - Color.DWColor();
	Diff[EInstrumentsColors::Purple] = FColor::Purple.DWColor() - Color.DWColor();
	Diff[EInstrumentsColors::Orange] = FColor::Orange.DWColor() - Color.DWColor();
	Diff[EInstrumentsColors::Red] = FColor::Red.DWColor() - Color.DWColor();
	
	uint32 Min = 0xffffffff;
	uint32 Index = 0;
	for(uint32 i = 0; i < EInstrumentsColors::Max; i++)
	{
		if (Diff[i] < Min)
		{
			Min = Diff[i];
			Index = i;
		}
	}
	
	return Index;
}

void FApplePlatformDebugEvents::DebugSignPost(uint16 Code, uintptr_t Arg1, uintptr_t Arg2, uintptr_t Arg3, uintptr_t Arg4)
{
	if (GAppleInstrumentsEvents)
	{
#if APPLE_PROFILING_FALLBACKS
	#if PLATFORM_MAC
		static NSOperatingSystemVersion Version = {10,12,0};
	#else
		static NSOperatingSystemVersion Version = {10,0,0};
	#endif
		static bool bKDebugAvail = [[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:Version];
		if (!bKDebugAvail)
		{
			syscall(SYS_kdebug_trace, APPSDBG_CODE(DBG_MACH_CHUD, Code) | DBG_FUNC_NONE, Arg1, Arg2, Arg3, Arg4);
		}
		else
#endif
		{
			kdebug_signpost(Code, Arg1, Arg2, Arg3, Arg4);
		}
	}
}

void FApplePlatformDebugEvents::DebugSignPostStart(uint16 Code, uintptr_t Arg1, uintptr_t Arg2, uintptr_t Arg3, uintptr_t Arg4)
{
	if (GAppleInstrumentsEvents)
	{
#if APPLE_PROFILING_FALLBACKS
	#if PLATFORM_MAC
		static NSOperatingSystemVersion Version = {10,12,0};
	#else
		static NSOperatingSystemVersion Version = {10,0,0};
	#endif
		static bool bKDebugAvail = [[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:Version];
		if (!bKDebugAvail)
		{
			syscall(SYS_kdebug_trace, APPSDBG_CODE(DBG_MACH_CHUD, Code) | DBG_FUNC_START, Arg1, Arg2, Arg3, Arg4);
		}
		else
#endif
		{
			kdebug_signpost_start(Code, Arg1, Arg2, Arg3, Arg4);
		}
	}
}

void FApplePlatformDebugEvents::DebugSignPostEnd(uint16 Code, uintptr_t Arg1, uintptr_t Arg2, uintptr_t Arg3, uintptr_t Arg4)
{
	if (GAppleInstrumentsEvents)
	{
#if APPLE_PROFILING_FALLBACKS
	#if PLATFORM_MAC
		static NSOperatingSystemVersion Version = {10,12,0};
	#else
		static NSOperatingSystemVersion Version = {10,0,0};
	#endif
		static bool bKDebugAvail = [[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:Version];
		if (!bKDebugAvail)
		{
			syscall(SYS_kdebug_trace, APPSDBG_CODE(DBG_MACH_CHUD, Code) | DBG_FUNC_END, Arg1, Arg2, Arg3, Arg4);
		}
		else
#endif
		{
			kdebug_signpost_end(Code, Arg1, Arg2, Arg3, Arg4);
		}
	}
}

TArray<FApplePlatformDebugEvents::FEvent>* FApplePlatformDebugEvents::GetEventStack()
{
	TArray<FEvent>* Current = (TArray<FEvent>*)FPlatformTLS::GetTlsValue(TLSSlot);
	if (!Current)
	{
		Current = new TArray<FEvent>;
		FPlatformTLS::SetTlsValue(TLSSlot, Current);
	}
	check(Current);
	return Current;
}

uint16 FApplePlatformDebugEvents::GetEventCode(FString String)
{
	// Never emit 0 as we use that for the frame marker...
	uint16 Code = 1;
	
	if (String.StartsWith(TEXT("Frame")))
	{
		String = TEXT("Frame");
	}
	else if (String.StartsWith(TEXT("PerObject")))
	{
		String = TEXT("PerObject");
	}
	else if (String.StartsWith(TEXT("PreShadow")))
	{
		String = TEXT("PreShadow");
	}
	
	uint32 Hash = GetTypeHash(String);
	
	static FRWLock RWMutex;
	static TMap<uint32, uint16> Names;
	{
		FRWScopeLock Lock(RWMutex, SLT_ReadOnly);
		uint16* CodePtr = Names.Find(Hash);
		if(CodePtr)
		{
			Code = *CodePtr;
		}
		
		if(!CodePtr)
		{
			Lock.RaiseLockToWrite();
			CodePtr = Names.Find(Hash);
			if(CodePtr)
			{
				Code = *CodePtr;
			}
			else
			{
				Code = Names.Num() + 1;
				check(Code < 16384);
				Names.Add(Hash, Code);
				UE_LOG(LogInstruments, Display, TEXT("New Event Code: %u : %s"), (uint32)Code, *String);
			}
		}
	}
	
	return Code;
}

void FApplePlatformDebugEvents::BeginNamedEvent(const struct FColor& Color,const TCHAR* Text)
{
	if (GAppleInstrumentsEvents)
	{
		FString Name(Text);
		
		FEvent Event;
		Event.Tag = Text;
		Event.Color = GetInstrumentsColor(Color);
		Event.Code = GetEventCode(Name);
		
		GetEventStack()->Add(Event);
		DebugSignPostStart(Event.Code, (uintptr_t)Event.Tag, 0, 0, (uintptr_t)Event.Color);
	}
}

void FApplePlatformDebugEvents::BeginNamedEvent(const struct FColor& Color,const ANSICHAR* Text)
{
	if (GAppleInstrumentsEvents)
	{
		FString Name(Text);
		
		FEvent Event;
		Event.Tag = Text;
		Event.Color = GetInstrumentsColor(Color);
		Event.Code = GetEventCode(Name);
		
		GetEventStack()->Add(Event);
		DebugSignPostStart(Event.Code, (uintptr_t)Event.Tag, 0, 0, (uintptr_t)Event.Color);
	}
}

void FApplePlatformDebugEvents::EndNamedEvent()
{
	if (GAppleInstrumentsEvents)
	{
		TArray<FEvent>* Stack = GetEventStack();
		FEvent Last = Stack->Last();
		Stack->RemoveAt(Stack->Num() - 1);
		DebugSignPostEnd(Last.Code, (uintptr_t)Last.Tag, 0, 0, (uintptr_t)Last.Color);
	}
}

#endif
