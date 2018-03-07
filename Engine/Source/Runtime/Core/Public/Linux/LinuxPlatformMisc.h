// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformMisc.h: Linux platform misc functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Linux/LinuxSystemIncludes.h"
#include "Misc/Build.h"

class Error;
struct FGenericCrashContext;

/**
 * Linux implementation of the misc OS functions
 */
struct CORE_API FLinuxPlatformMisc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static void PlatformTearDown();
	static void SetGracefulTerminationHandler();
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context));
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);
	static void SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value);
	DEPRECATED(4.14, "GetMacAddress is deprecated. It is not reliable on all platforms")
	static TArray<uint8> GetMacAddress();
	static bool IsRunningOnBattery();

#if !UE_BUILD_SHIPPING
	static bool IsDebuggerPresent();
	static void DebugBreak();
#endif // !UE_BUILD_SHIPPING

	/** Break into debugger. Returning false allows this function to be used in conditionals. */
	FORCEINLINE static bool DebugBreakReturningFalse()
	{
#if !UE_BUILD_SHIPPING
		DebugBreak();
#endif
		return false;
	}

	/** Prompts for remote debugging if debugger is not attached. Regardless of result, breaks into debugger afterwards. Returns false for use in conditionals. */
	static FORCEINLINE bool DebugBreakAndPromptForRemoteReturningFalse(bool bIsEnsure = false)
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

	static void LowLevelOutputDebugString(const TCHAR *Message);

	static void RequestExit(bool Force);
	static void RequestExitWithStatus(bool Force, uint8 ReturnCode);
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);

	static void NormalizePath(FString& InPath);

	static const TCHAR* GetPathVarDelimiter()
	{
		return TEXT(":");
	}

	static EAppReturnType::Type MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption);

	FORCEINLINE static void MemoryBarrier()
	{
		__sync_synchronize();
	}

	FORCEINLINE static void PrefetchBlock(const void* InPtr, int32 NumBytes = 1)
	{
		extern size_t GCacheLineSize;

		const char* Ptr = static_cast<const char*>(InPtr);
		const size_t CacheLineSize = GCacheLineSize;
		for (size_t BytesPrefetched = 0; BytesPrefetched < NumBytes; BytesPrefetched += CacheLineSize)
		{
			__builtin_prefetch(Ptr);
			Ptr += CacheLineSize;
		}
	}

	FORCEINLINE static void Prefetch(void const* Ptr, int32 Offset = 0)
	{
		__builtin_prefetch(static_cast<char const*>(Ptr) + Offset);
	}

	static int32 NumberOfCores();
	static int32 NumberOfCoresIncludingHyperthreads();
	static FString GetOperatingSystemId();
	static bool GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes);

	/**
	 * Determines the shader format for the platform
	 *
	 * @return	Returns the shader format to be used by that platform
	 */
	static const TCHAR* GetNullRHIShaderFormat();

	static bool HasCPUIDInstruction();
	static FString GetCPUVendor();
	static FString GetCPUBrand();

	/**
	 * Uses cpuid instruction to get the vendor string
	 *
	 * @return	CPU info bitfield
	 *
	 *			Bits 0-3	Stepping ID
	 *			Bits 4-7	Model
	 *			Bits 8-11	Family
	 *			Bits 12-13	Processor type (Intel) / Reserved (AMD)
	 *			Bits 14-15	Reserved
	 *			Bits 16-19	Extended model
	 *			Bits 20-27	Extended family
	 *			Bits 28-31	Reserved
	 */
	static uint32 GetCPUInfo();

	static bool HasNonoptionalCPUFeatures();
	static bool NeedsNonoptionalCPUFeaturesCheck();

#if !UE_BUILD_SHIPPING	// only in non-shipping because we break into the debugger in non-shipping builds only
	/**
	 * Ungrabs input (useful before breaking into debugging)
	 */
	static void UngrabAllInput();
#endif // !UE_BUILD_SHIPPING

	/**
	 * Returns whether the program has been started remotely (e.g. over SSH)
	 */
	static bool HasBeenStartedRemotely();

	/**
	 * Determines if return code has been overriden and returns it.
	 *
	 * @param OverriddenReturnCodeToUsePtr pointer to an variable that will hold an overriden return code, if any. Can be null.
	 *
	 * @return true if the error code has been overriden, false if not
	 */
	static bool HasOverriddenReturnCode(uint8 * OverriddenReturnCodeToUsePtr);
	static FString GetOSVersion();
};

typedef FLinuxPlatformMisc FPlatformMisc;
