// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMemory.h"
#include "GenericPlatform/GenericPlatformMisc.h"

class GenericApplication;
struct FGuid;
struct FVector2D;
class IPlatformChunkInstall;

/** Helper struct used to get the string version of the Windows version. */
struct CORE_API FWindowsOSVersionHelper
{
	enum ErrorCodes
	{
		SUCCEEDED = 0,
		ERROR_UNKNOWNVERSION = 1,
		ERROR_GETPRODUCTINFO_FAILED = 2,
		ERROR_GETVERSIONEX_FAILED = 4,
		ERROR_GETWINDOWSGT62VERSIONS_FAILED = 8,
	};

	static int32 GetOSVersions( FString& out_OSVersion, FString& out_OSSubVersion );
	static FString GetOSVersion();
};


/**
* Windows implementation of the misc OS functions
**/
struct CORE_API FWindowsPlatformMisc
	: public FGenericPlatformMisc
{
	static void SetHighDPIMode();
	static void PlatformPreInit();
	static void PlatformInit();
	static void SetGracefulTerminationHandler();
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);
	static void SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value);

	static TArray<uint8> GetMacAddress();
	static void SubmitErrorReport( const TCHAR* InErrorHist, EErrorReportMode::Type InMode );

#if !UE_BUILD_SHIPPING
	static bool IsDebuggerPresent();
	FORCEINLINE static void DebugBreak()
	{
		if (IsDebuggerPresent())
		{
			// Prefer __debugbreak() instead of ::DebugBreak() on Windows platform, so the code pointer isn't left
			// inside the DebugBreak() Windows system library (which we usually won't have symbols for), and avoids
			// us having to "step out" to get back to Unreal code.
			__debugbreak();
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

	static void SetUTF8Output();
	static void LocalPrint(const TCHAR *Message);
	static void RequestExit(bool Force);
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);
	static void CreateGuid(struct FGuid& Result);
	static EAppReturnType::Type MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );
	static bool CommandLineCommands();
	static bool Is64bitOperatingSystem();
	static bool IsValidAbsolutePathFormat(const FString& Path);
	static int32 NumberOfCores();
	static int32 NumberOfCoresIncludingHyperthreads();

	static FString GetDefaultLanguage();
	static FString GetDefaultLocale();

	static uint32 GetLastError();
	static void RaiseException( uint32 ExceptionCode );
	static bool SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue);
	static bool GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue);
	static bool DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName);

	static bool CoInitialize();
	static void CoUninitialize();

	/**
	 * Has the OS execute a command and path pair (such as launch a browser)
	 *
	 * @param ComandType OS hint as to the type of command 
	 * @param Command the command to execute
	 * @param CommandLine the commands to pass to the executable
	 *
	 * @return whether the command was successful or not
	 */
	static bool OsExecute(const TCHAR* CommandType, const TCHAR* Command, const TCHAR* CommandLine = NULL);

	/**
	 * Attempts to get the handle to a top-level window of the specified process.
	 *
	 * If the process has a single main window (root), its handle will be returned.
	 * If the process has multiple top-level windows, the first one found is returned.
	 *
	 * @param ProcessId The identifier of the process to get the window for.
	 * @return Window handle, or 0 if not found.
	 */
	static Windows::HWND GetTopLevelWindowHandle(uint32 ProcessId);

	/** 
	 * Determines if we are running on the Windows version or newer
	 *
	 * See the 'Remarks' section of https://msdn.microsoft.com/en-us/library/windows/desktop/ms724833(v=vs.85).aspx
	 * for a list of MajorVersion/MinorVersion version combinations for Microsoft Windows.
	 *
	 * @return	Returns true if the current Windows version if equal or newer than MajorVersion
	 */
	static bool VerifyWindowsVersion(uint32 MajorVersion, uint32 MinorVersion);

#if !UE_BUILD_SHIPPING
	static void PromptForRemoteDebugging(bool bIsEnsure);
#endif	//#if !UE_BUILD_SHIPPING

	FORCEINLINE static void PrefetchBlock(const void* InPtr, int32 NumBytes = 1)
	{
		const char* Ptr           = (const char*)InPtr;
		const int32 CacheLineSize = GetCacheLineSize();
		for (int32 LinesToPrefetch = (NumBytes + CacheLineSize - 1) / CacheLineSize; LinesToPrefetch; --LinesToPrefetch)
		{
			_mm_prefetch( Ptr, _MM_HINT_T0 );
			Ptr += CacheLineSize;
		}
	}

	FORCEINLINE static void Prefetch(void const* x, int32 offset = 0)
	{
		 _mm_prefetch( (char const*)(x) + offset, _MM_HINT_T0 );
	}

	/** 
	 * Determines if the cpuid instruction is supported on this processor
	 *
	 * @return	Returns true if cpuid is supported
	 */
	static bool HasCPUIDInstruction();

	static FString GetCPUVendor();
	static FString GetCPUBrand();
	static FString GetPrimaryGPUBrand();
	static struct FGPUDriverInfo GetGPUDriverInfo(const FString& DeviceDescription);
	static void GetOSVersions( FString& out_OSVersionLabel, FString& out_OSSubVersionLabel );
	static FString GetOSVersion();
	static bool GetDiskTotalAndFreeSpace( const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes );

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

	/** @return whether this cpu supports certain required instructions or not */
	static bool HasNonoptionalCPUFeatures();
	/** @return whether to check for specific CPU compatibility or not */
	static bool NeedsNonoptionalCPUFeaturesCheck();

	/** 
	 * Provides a simpler interface for fetching and cleanup of registry value queries
	 *
	 * @param	InKey		The Key (folder) in the registry to search under
	 * @param	InSubKey	The Sub Key (folder) within the main Key to look for
	 * @param	InValueName	The Name of the Value (file) withing the Sub Key to look for
	 * @param	OutData		The Data entered into the Value
	 *
	 * @return	true, if it successfully found the Value
	 */
	static bool QueryRegKey( const Windows::HKEY InKey, const TCHAR* InSubKey, const TCHAR* InValueName, FString& OutData );

	/**
	 * Gets Visual Studio common tools path.
	 *
	 * @param Version Version of VS to get (11 - 2012, 12 - 2013).
	 * @param OutData Output parameter with common tools path.
	 *
	 * @return Returns if succeeded.
	 */
	static bool GetVSComnTools(int32 Version, FString& OutData);

	/**
	 * Returns the size of the cache line in bytes.
	 *
	 * @return The cache line size.
	 */
	static int32 GetCacheLineSize();

	/**
	* @return Windows path separator.
	*/
	static const TCHAR* GetDefaultPathSeparator();

	/** @return Get the name of the platform specific file manager (Explorer) */
	static FText GetFileManagerName();

	/**
	* Returns whether WiFi connection is currently active
	*/
	static bool HasActiveWiFiConnection()
	{
		// for now return true
		return true;
	}

	/**
	 * Returns whether the platform is running on battery power or not.
	 */
	static bool IsRunningOnBattery();

	/**
	 * Gets a globally unique ID the represents a particular operating system install.
	 */
	static FString GetOperatingSystemId();

	static EConvertibleLaptopMode GetConvertibleLaptopMode();

	static IPlatformChunkInstall* GetPlatformChunkInstall();
	
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	static void LoadVxgiModule();
	static void UnloadVxgiModule();
#endif
	// NVCHANGE_END: Add VXGI
};


typedef FWindowsPlatformMisc FPlatformMisc;
