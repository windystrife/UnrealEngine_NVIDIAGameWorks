// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformMisc.h: Apple platform misc functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#else
#include "IOS/IOSSystemIncludes.h"
#endif

#ifndef APPLE_PROFILING_ENABLED
#define APPLE_PROFILING_ENABLED (UE_BUILD_DEBUG | UE_BUILD_DEVELOPMENT)
#endif

#ifndef WITH_SIMULATOR
#define WITH_SIMULATOR 0
#endif

#ifdef __OBJC__

class FScopeAutoreleasePool
{
public:

	FScopeAutoreleasePool()
	{
		Pool = [[NSAutoreleasePool alloc] init];
	}

	~FScopeAutoreleasePool()
	{
		[Pool release];
	}

private:

	NSAutoreleasePool*	Pool;
};

#define SCOPED_AUTORELEASE_POOL const FScopeAutoreleasePool PREPROCESSOR_JOIN(Pool,__LINE__);

#endif // __OBJC__

/**
* Apple implementation of the misc OS functions
**/
struct CORE_API FApplePlatformMisc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);

#if !UE_BUILD_SHIPPING
	static bool IsDebuggerPresent()
	{
		// Based on http://developer.apple.com/library/mac/#qa/qa1361/_index.html

		struct kinfo_proc Info;
		int32 Mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
		SIZE_T Size = sizeof(Info);

		sysctl( Mib, sizeof( Mib ) / sizeof( *Mib ), &Info, &Size, NULL, 0 );

		return ( Info.kp_proc.p_flag & P_TRACED ) != 0;
	}
	FORCEINLINE static void DebugBreak()
	{
		if( IsDebuggerPresent() )
		{
			//Signal interupt to our process, this is caught by the main thread, this is not immediate but you can continue
			//when triggered by check macros you will need to inspect other threads for the appFailAssert call.
			//kill( getpid(), SIGINT );

			//If you want an immediate break use the trap instruction, continued execuction is halted
#if WITH_SIMULATOR || PLATFORM_MAC
            __asm__ ( "int $3" );
#elif PLATFORM_64BITS
			__asm__ ( "svc 0" );
#else
            __asm__ ( "trap" );
#endif
		}
	}
#endif

	/** Break into debugger. Returning false allows this function to be used in conditionals. */
	FORCEINLINE static bool DebugBreakReturningFalse()
	{
#if !UE_BUILD_SHIPPING
		DebugBreak();
#endif
		return false;
	}

	/** Prompts for remote debugging if debugger is not attached. Regardless of result, breaks into debugger afterwards. Returns false for use in conditionals. */
	FORCEINLINE static bool DebugBreakAndPromptForRemoteReturningFalse(bool bIsEnsure = false)
	{
#if !UE_BUILD_SHIPPING
		if (!IsDebuggerPresent())
		{
			PromptForRemoteDebugging(bIsEnsure);
		}

		DebugBreak();
#endif

		return false;
	}

	FORCEINLINE static void MemoryBarrier()
	{
		__sync_synchronize();
	}

	static void LowLevelOutputDebugString(const TCHAR *Message);
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);
	static int32 NumberOfCores();
	
	static void CreateGuid(struct FGuid& Result);
	static TArray<uint8> GetSystemFontBytes();
	static FString GetDefaultLocale();
	static FString GetDefaultLanguage();
	static TArray<FString> GetPreferredLanguages();
	static FString GetLocalCurrencyCode();
	static FString GetLocalCurrencySymbol();

	static bool IsOSAtLeastVersion(const uint32 MacOSVersion[3], const uint32 IOSVersion[3], const uint32 TVOSVersion[3]);

#if APPLE_PROFILING_ENABLED
	static void BeginNamedEvent(const struct FColor& Color,const TCHAR* Text);
	static void BeginNamedEvent(const struct FColor& Color,const ANSICHAR* Text);
	static void EndNamedEvent();
#endif
	
	//////// Platform specific
	static void* CreateAutoreleasePool();
	static void ReleaseAutoreleasePool(void *Pool);
};
