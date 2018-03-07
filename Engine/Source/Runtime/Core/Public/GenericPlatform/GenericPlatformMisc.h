// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"
#include "HAL/PlatformCrt.h"

class Error;
class GenericApplication;
class IPlatformChunkInstall;
class IPlatformCompression;
struct FGenericCrashContext;
struct FGenericMemoryWarningContext;

namespace EBuildConfigurations
{
	/**
	 * Enumerates build configurations.
	 */
	enum Type
	{
		/** Unknown build configuration. */
		Unknown,

		/** Debug build. */
		Debug,

		/** DebugGame build. */
		DebugGame,

		/** Development build. */
		Development,

		/** Shipping build. */
		Shipping,

		/** Test build. */
		Test
	};

	/**
	 * Returns the string representation of the specified EBuildConfiguration value.
	 *
	 * @param Configuration The string to get the EBuildConfiguration::Type for.
	 * @return An EBuildConfiguration::Type value.
	 */
	CORE_API EBuildConfigurations::Type FromString( const FString& Configuration );

	/**
	 * Returns the string representation of the specified EBuildConfiguration value.
	 *
	 * @param Configuration The value to get the string for.
	 * @return The string representation.
	 */
	CORE_API const TCHAR* ToString( EBuildConfigurations::Type Configuration );

	/**
	 * Returns the localized text representation of the specified EBuildConfiguration value.
	 *
	 * @param Configuration The value to get the text for.
	 * @return The localized Build configuration text
	 */
	CORE_API FText ToText( EBuildConfigurations::Type Configuration );
}


namespace EBuildTargets
{
	/**
	 * Enumerates build targets.
	 */
	enum Type
	{
		/** Unknown build target. */
		Unknown,

		/** Editor target. */
		Editor,

		/** Game target. */
		Game,

		/** Server target. */
		Server
	};

	/**
	 * Returns the string representation of the specified EBuildTarget value.
	 *
	 * @param Target The string to get the EBuildTarget::Type for.
	 * @return An EBuildTarget::Type value.
	 */
	CORE_API EBuildTargets::Type FromString( const FString& Target );

	/**
	 * Returns the string representation of the specified EBuildTarget value.
	 *
	 * @param Target The value to get the string for.
	 * @return The string representation.
	 */
	CORE_API const TCHAR* ToString( EBuildTargets::Type Target );
}


/**
 * Enumerates the modes a convertible laptop can be in.
 */
enum class EConvertibleLaptopMode
{
	/** Not a convertible laptop. */
	NotSupported,

	/** Laptop arranged as a laptop. */
	Laptop,

	/** Laptop arranged as a tablet. */
	Tablet
};

/** Device orientations for screens. e.g. Landscape, Portrait, etc.*/
enum class EDeviceScreenOrientation : uint8
{
	/** The orientation is not known */
	Unknown,
	
	/** The orientation is portrait with the home button at the bottom */
	Portrait,
	
	/** The orientation is portrait with the home button at the top */
	PortraitUpsideDown,
	
	/** The orientation is landscape with the home button at the right side */
	LandscapeLeft,
	
	/** The orientation is landscape with the home button at the left side */
	LandscapeRight,
	
	/** The orientation is as if place on a desk with the screen upward */
	FaceUp,
	
	/** The orientation is as if place on a desk with the screen downward */
	FaceDown
};


namespace EErrorReportMode
{
	/** 
	 * Enumerates supported error reporting modes.
	 */
	enum Type
	{
		/** Displays a call stack with an interactive dialog for entering repro steps, etc. */
		Interactive,

		/** Unattended mode.  No repro steps, just submits data straight to the server */
		Unattended,

		/** Same as unattended, but displays a balloon window in the system tray to let the user know */
		Balloon,
	};
}


namespace EAppMsgType
{
	/**
	 * Enumerates supported message dialog button types.
	 */
	enum Type
	{
		Ok,
		YesNo,
		OkCancel,
		YesNoCancel,
		CancelRetryContinue,
		YesNoYesAllNoAll,
		YesNoYesAllNoAllCancel,
		YesNoYesAll,
	};
}


namespace EAppReturnType
{
	/**
	 * Enumerates message dialog return types.
	 */
	enum Type
	{
		No,
		Yes,
		YesAll,
		NoAll,
		Cancel,
		Ok,
		Retry,
		Continue,
	};
}

/*
 * Holds a computed SHA256 hash.
 */
struct CORE_API FSHA256Signature
{
	uint8 Signature[32];

	/** Generates a hex string of the signature */
	FString ToString() const;
};

/**
* Generic implementation for most platforms
**/
struct CORE_API FGenericPlatformMisc
{
	/**
	 * Called during appInit() after cmd line setup
	 */
	static void PlatformPreInit();
	static void PlatformInit() { }

	/**
	* Called to dismiss splash screen
	*/
	static void PlatformHandleSplashScreen(bool ShowSplashScreen = false) { }

	/**
	 * Called during AppExit(). Log, Config still exist at this point, but not much else does.
	 */
	static void PlatformTearDown() { }

	/** Set/restore the Console Interrupt (Control-C, Control-Break, Close) handler. */
	static void SetGracefulTerminationHandler() { }

	/**
	 * Installs handler for the unexpected (due to error) termination of the program,
	 * including, but not limited to, crashes.
	 */
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context)) { }

	/**
	 * Retrieve a environment variable from the system
	 *
	 * @param VariableName The name of the variable (ie "Path")
	 * @param Result The string to copy the value of the variable into
	 * @param ResultLength The size of the Result string
	 */
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
	{
		*Result = 0;
	}


	/**
	 * Sets an environment variable to the local process's environment
	 *
	 * @param VariableName The name of the variable (ie "Path")
	 * @param Value The string to set the variable to.	
	 */
	static void SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value);

	/**
	 * return the delimiter between paths in the PATH environment variable.
	 */
	static const TCHAR* GetPathVarDelimiter();

	/**
	 * Retrieve the Mac address of the current adapter.
	 * 
	 * @return array of bytes representing the Mac address, or empty array if unable to determine.
	 */
	DEPRECATED(4.14, "GetMacAddress is deprecated. It is not reliable on all platforms")
	static TArray<uint8> GetMacAddress();

	/**
	 * Retrieve the Mac address of the current adapter as a string.
	 * 
	 * @return String representing the Mac address, or empty string.
	 */
	DEPRECATED(4.14, "GetMacAddressString is deprecated. It is not reliable on all platforms")
	static FString GetMacAddressString();

	/**
	 * Retrieve the Mac address of the current adapter as a hashed string (privacy)
	 *
	 * @return String representing the hashed Mac address, or empty string.
	 */
	DEPRECATED(4.14, "GetHasedMacAddressString is deprecated. It is not reliable on all platforms")
	static FString GetHashedMacAddressString();

	/**
	 * Returns a unique string for device identification
	 * 
	 * @return the unique string generated by this platform for this device
	 */
	DEPRECATED(4.14, "GetUniqueDeviceId is deprecated. Use GetDeviceId instead.")
	static FString GetUniqueDeviceId();

	/**
	 * Returns a unique string for device identification. Differs from the deprecated GetUniqueDeviceId
	 * in that there is no default implementation (which used unreliable Mac address determiniation).
	 * This code is expected to use platform-specific methods to identify the device.
	 * 
	 * WARNING: Use of this method in your app may imply technical certification requirments for your platform!
	 * For instance, consoles often require cert waivers to be in place before calling APIs that can track a device,
	 * so be very careful that you are following your platform's protocols for accessing device IDs. See the platform-
	 * specific implementations of this method for details on what APIs are used.
	 *
	 * @return the unique string generated by this platform for this device, or an empty string if one is not available.
	 */
	static FString GetDeviceId();

	/**
	* Returns a unique string for advertising identification
	*
	* @return the unique string generated by this platform for this device
	*/
	static FString GetUniqueAdvertisingId();

	// #CrashReport: 2015-02-24 Remove
	/** Submits a crash report to a central server (release builds only) */
	static void SubmitErrorReport( const TCHAR* InErrorHist, EErrorReportMode::Type InMode );

	/** Return true if a debugger is present */
	FORCEINLINE static bool IsDebuggerPresent()
	{
#if UE_BUILD_SHIPPING
		return 0; 
#else
		return 1; // unknown platforms return true so that they can crash into a debugger
#endif
	}

	/** Break into the debugger, if IsDebuggerPresent returns true, otherwise do nothing  */
	FORCEINLINE static void DebugBreak()
	{
		if (IsDebuggerPresent())
		{
			*((int32*)3) = 13; // unknown platforms crash into the debugger
		}
	}

	/**
	* Uses cpuid instruction to get the vendor string
	*
	* @return	CPU vendor name
	*/
	static FString GetCPUVendor();

	/**
	 * On x86(-64) platforms, uses cpuid instruction to get the CPU signature
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
	 * Uses cpuid instruction to get the CPU brand string
	 *
	 * @return	CPU brand string
	 */
	static FString GetCPUBrand();

	/** 
	 * @return primary GPU brand string
	 */
	static FString GetPrimaryGPUBrand();

	/**
	 * @return	"DeviceMake|DeviceModel" if possible, and "CPUVendor|CPUBrand" otherwise
	 */
	static FString GetDeviceMakeAndModel();

	static struct FGPUDriverInfo GetGPUDriverInfo(const FString& DeviceDescription);

	/**
	 * Gets the OS Version and OS Subversion.
	 */
	static void GetOSVersions( FString& out_OSVersionLabel, FString& out_OSSubVersionLabel );

	/**
	 * Gets a string representing the numeric OS version (as opposed to a translated OS version that GetOSVersions returns).
	 * The returned string should try to be brief and avoid newlines and symbols, but there's technically no restriction on the string it can return.
	 * If the implementation does not support this, it should return an empty string.
	 */
	static FString GetOSVersion();

	/** Retrieves information about the total number of bytes and number of free bytes for the specified disk path. */
	static bool GetDiskTotalAndFreeSpace( const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes );

	static bool SupportsMessaging()
	{
		return true;
	}

	static bool SupportsLocalCaching()
	{
		return true;
	}

	/** Platform can generate a full-memory crashdump during crash handling. */
	static bool SupportsFullCrashDumps()
	{
		return true;
	}

	/**
	 * Enforces strict memory load/store ordering across the memory barrier call.
	 */
	FORCENOINLINE static void MemoryBarrier();

	/**
	 * Handles IO failure by ending gameplay.
	 *
	 * @param Filename	If not nullptr, name of the file the I/O error occured with
	 */
	void static HandleIOFailure( const TCHAR* Filename );

	/**
	 * Set a handler to be called when there is a memory warning from the OS 
	 *
	 * @param Handler	The handler to call
	 */
	static void SetMemoryWarningHandler(void (* Handler)(const FGenericMemoryWarningContext& Context))
	{
	}

	FORCEINLINE static uint32 GetLastError()
	{
		return 0;
	}

	static void RaiseException( uint32 ExceptionCode );

public:

	/**
	 * Platform specific function for adding a named event that can be viewed in PIX
	 */
	FORCEINLINE static void BeginNamedEvent(const struct FColor& Color,const TCHAR* Text)
	{
	}

	FORCEINLINE static void BeginNamedEvent(const struct FColor& Color,const ANSICHAR* Text)
	{
	}

	/**
	 * Platform specific function for closing a named event that can be viewed in PIX
	 */
	FORCEINLINE static void EndNamedEvent()
	{
	}

    /**
	* Platform specific function for initializing storage of tagged memory buffers
	*/
	FORCEINLINE static void InitTaggedStorage(uint32 NumTags)
	{
	}

    /**
	* Platform specific function for freeing storage of tagged memory buffers
	*/
	FORCEINLINE static void ShutdownTaggedStorage()
	{
	}

    /**
	* Platform specific function for tagging a memory buffer with a label. Helps see memory access in profilers
	*/
	FORCEINLINE static void TagBuffer(const char* Label, uint32 Category, const void* Buffer, size_t BufferSize)
	{
	}

	/** 
	 *	Set the value for the given section and key in the platform specific key->value store
	 *  Note: The key->value store is user-specific, but may be used to share data between different applications for the same user
	 *
	 *  @param	InStoreId			The name used to identify the store you want to use (eg, MyGame)
	 *	@param	InSectionName		The section that this key->value pair is placed within (can contain / separators, eg UserDetails/AccountInfo)
	 *	@param	InKeyName			The name of the key to set the value for
	 *	@param	InValue				The value to set
	 *	@return	bool				true if the value was set correctly, false if not
	 */
	static bool SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue);

	/** 
	 *	Get the value for the given section and key from the platform specific key->value store
	 *  Note: The key->value store is user-specific, but may be used to share data between different applications for the same user
	 *
	 *  @param	InStoreId			The name used to identify the store you want to use (eg, MyGame)
	 *	@param	InSectionName		The section that this key->value pair is placed within (can contain / separators, eg UserDetails/AccountInfo)
	 *	@param	InKeyName			The name of the key to get the value for
	 *	@param	OutValue			The value found
	 *	@return	bool				true if the entry was found (and OutValue contains the result), false if not
	 */
	static bool GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue);

	/**
  	 *	Deletes value for the given section and key in the platform specific key->value store
	 *  Note: The key->value store is user-specific, but may be used to share data between different applications for the same user
	 *
	 *  @param	InStoreId			The name used to identify the store you want to use (eg, MyGame)
	 *	@param	InSectionName		The section that this key->value pair is placed within (can contain / separators, eg UserDetails/AccountInfo)
	 *	@param	InKeyName			The name of the key to set the value for
	 *	@return	bool				true if the value was deleted correctly, false if not found or couldn't delete
	 */
	static bool DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName);

	/** Sends a message to a remote tool, and debugger consoles */
	static void LowLevelOutputDebugString(const TCHAR *Message);
	static void VARARGS LowLevelOutputDebugStringf(const TCHAR *Format, ... );

	/** Sets the default output to UTF8 */
	static void SetUTF8Output();

	/** Prints string to the default output */
	static void LocalPrint( const TCHAR* Str );

	/** 
	 * Whether the platform has a separate debug channel to stdout (eg. OutputDebugString on Windows). Used to suppress messages being output twice 
	 * if both go to the same place.
	 */
	static bool HasSeparateChannelForDebugOutput();

	/**
	 * Requests application exit.
	 *
	 * @param	Force	If true, perform immediate exit (dangerous because config code isn't flushed, etc).
	 *                  If false, request clean main-loop exit from the platform specific code.
	 */
	static void RequestExit( bool Force );

	/**
	 * Requests application exit with a specified return code. Name is different from RequestExit() so overloads of just one of functions are possible.
	 *
	 * @param	Force 	   If true, perform immediate exit (dangerous because config code isn't flushed, etc).
	 *                     If false, request clean main-loop exit from the platform specific code.
	 * @param   ReturnCode This value will be returned from the program (on the platforms where it's possible). Limited to 0-255 to conform with POSIX.
	 */
	static void RequestExitWithStatus( bool Force, uint8 ReturnCode );

	/**
	 * Returns the last system error code in string form.  NOTE: Only one return value is valid at a time!
	 *
	 * @param OutBuffer the buffer to be filled with the error message
	 * @param BufferLength the size of the buffer in character count
	 * @param Error the error code to convert to string form
	 */
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);

	/** Copies text to the operating system clipboard. */
	DEPRECATED(4.18, "FPlatformMisc::ClipboardCopy() has been superseded by FPlatformApplicationMisc::ClipboardCopy()")
	static void ClipboardCopy(const TCHAR* Str);

	/** Pastes in text from the operating system clipboard. */
	DEPRECATED(4.18, "FPlatformMisc::ClipboardPaste() has been superseded by FPlatformApplicationMisc::ClipboardPaste()")
	static void ClipboardPaste(class FString& Dest);

	/** Create a new globally unique identifier. **/
	static void CreateGuid(struct FGuid& Result);

	/** 
	 * Show a message box if possible, otherwise print a message and return the default
	 * @param MsgType What sort of options are provided
	 * @param Text Specific message
	 * @param Caption String indicating the title of the message box
	 * @return Very strange convention...not really EAppReturnType, see implementation
	 */
	static EAppReturnType::Type MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );

	/**
	 * Handles Game Explorer, Firewall and FirstInstall commands, typically from the installer
	 * @returns false if the game cannot continue.
	 */
	static bool CommandLineCommands()
	{
		return 1;
	}

	/**
	 * Detects whether we're running in a 64-bit operating system.
	 *
	 * @return	true if we're running in a 64-bit operating system
	 */
	FORCEINLINE static bool Is64bitOperatingSystem()
	{
		return !!PLATFORM_64BITS;
	}

	/**
	 * Checks structure of the path against platform formatting requirements
	 *
	 * return true if path is formatted validly
	 */
	static bool IsValidAbsolutePathFormat(const FString& Path)
	{
		return 1;
	}

	/** 
	 * Platform-specific normalization of path
	 * E.g. on Linux/Unix platforms, replaces ~ with user home directory, so ~/.config becomes /home/joe/.config (or /Users/Joe/.config)
	 */
	static void NormalizePath(FString& InPath)
	{
	}

	/**
	 * @return platform specific path separator.
	 */
	static const TCHAR* GetDefaultPathSeparator();

	/**
	 * Checks if platform wants to allow a rendering thread on current device (note: does not imply it will, only if okay given other criteria met)
	 *
	 * @return true if allowed, false if shouldn't use a separate rendering thread
	 */
	static bool AllowRenderThread()
	{
		// allow if not overridden
		return true;
	}

	/**
	* Checks if platform wants to allow an audio thread on current device (note: does not imply it will, only if okay given other criteria met)
	*
	* @return true if allowed, false if shouldn't use a separate audio thread
	*/
	static bool AllowAudioThread()
	{
		// allow if not overridden
		return true;
	}

	/**
	 * Checks if platform wants to allow the thread heartbeat hang detection
	 *
	 * @return true if allows, false if shouldn't allow thread heartbeat hang detection
	 */
	static bool AllowThreadHeartBeat();

	/**
	 * return the number of hardware CPU cores
	 */
	static int32 NumberOfCores()
	{
		return 1;
	}

	/**
	 * return the number of logical CPU cores
	 */
	static int32 NumberOfCoresIncludingHyperthreads();

	/**
	 * Return the number of worker threads we should spawn, based on number of cores
	 */
	static int32 NumberOfWorkerThreadsToSpawn();

	/**
	* Return the number of worker threads we should spawn to service IO, NOT based on number of cores
	*/
	static int32 NumberOfIOWorkerThreadsToSpawn();

	/**
	 * Return the platform specific async IO system, or nullptr if the standard one should be used.
	 */
	static struct FAsyncIOSystemBase* GetPlatformSpecificAsyncIOSystem()
	{
		return nullptr;
	}

	/** Return the name of the platform features module. Can be nullptr if there are no extra features for this platform */
	static const TCHAR* GetPlatformFeaturesModuleName()
	{
		// by default, no module
		return nullptr;
	}

	/** Get the application root directory. */
	static const TCHAR* RootDir();

	/** Get the engine directory */
	static const TCHAR* EngineDir();

	/** Get the directory the application was launched from (useful for commandline utilities) */
	static const TCHAR* LaunchDir();

	/** Function to store the current working directory for use with LaunchDir() */
	static void CacheLaunchDir();

	/**
	 *	Return the project directory
	 */
	static const TCHAR* ProjectDir();

	/**
	*	Return the CloudDir.  CloudDir can be per-user.
	*/
	static FString CloudDir();

	/**
	*	Return the GamePersistentDownloadDir.  
	*	On some platforms, returns the writable directory for downloaded data that persists across play sessions.
	*	This dir is always per-game.
	*/
	static const TCHAR* GamePersistentDownloadDir();

	static const TCHAR* GetUBTPlatform();

	static const TCHAR* GetUBTTarget();

	/** 
	 * Determines the shader format for the plarform
	 *
	 * @return	Returns the shader format to be used by that platform
	 */
	static const TCHAR* GetNullRHIShaderFormat();

	/** 
	 * Returns the platform specific chunk based install interface
	 *
	 * @return	Returns the platform specific chunk based install implementation
	 */
	static IPlatformChunkInstall* GetPlatformChunkInstall();

	/**
	 * Returns the platform specific compression interface
	 *
	 * @return Returns the platform specific compression interface
	 */
	static IPlatformCompression* GetPlatformCompression();

	/**
	 * Has the OS execute a command and path pair (such as launch a browser)
	 *
	 * @param ComandType OS hint as to the type of command 
	 * @param Command the command to execute
	 * @param CommandLine the commands to pass to the executable
	 * @return whether the command was successful or not
	 */
	static bool OsExecute(const TCHAR* CommandType, const TCHAR* Command, const TCHAR* CommandLine = NULL)
	{
		return false;
	}

	/**
	 * @return true if this build is meant for release to retail
	 */
	static bool IsPackagedForDistribution()
	{
#if UE_BUILD_SHIPPING
		return true;
#else
		return false;
#endif
	}

	/**
	* Generates the SHA256 signature of the given data.
	* 
	*
	* @param Data Pointer to the beginning of the data to hash
	* @param Bytesize Size of the data to has, in bytes.
	* @param OutSignature Output Structure to hold the computed signature. 
	*
	* @return whether the hash was computed successfully
	*/
	static bool GetSHA256Signature(const void* Data, uint32 ByteSize, FSHA256Signature& OutSignature);	

	/**
	 * Get the default language (for localization) used by this platform.
	 * @note This is typically the same as GetDefaultLocale unless the platform distinguishes between the two.
	 * @note This should be returned in IETF language tag form:
	 *  - A two-letter ISO 639-1 language code (eg, "zh").
	 *  - An optional four-letter ISO 15924 script code (eg, "Hans").
	 *  - An optional two-letter ISO 3166-1 country code (eg, "CN").
	 */
	static FString GetDefaultLanguage();

	/**
	 * Get the default locale (for internationalization) used by this platform.
	 * @note This should be returned in IETF language tag form:
	 *  - A two-letter ISO 639-1 language code (eg, "zh").
	 *  - An optional four-letter ISO 15924 script code (eg, "Hans").
	 *  - An optional two-letter ISO 3166-1 country code (eg, "CN").
	 */
	static FString GetDefaultLocale();

	/**
	 *	Platform-specific Exec function
	 *
	 *  @param	InWorld		World context
	 *	@param	Cmd			The command to execute
	 *	@param	Out			The output device to utilize
	 *	@return	bool		true if command was processed, false if not
	 */
	static bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out)
	{
		return false;
	}

	/** @return Get the name of the platform specific file manager (eg, Explorer on Windows, Finder on OS X) */
	static FText GetFileManagerName();

#if !UE_BUILD_SHIPPING
	static void SetShouldPromptForRemoteDebugging(bool bInShouldPrompt)
	{
		bShouldPromptForRemoteDebugging = bInShouldPrompt;
	}

	static void SetShouldPromptForRemoteDebugOnEnsure(bool bInShouldPrompt)
	{
		bPromptForRemoteDebugOnEnsure = bInShouldPrompt;
	}
#endif	//#if !UE_BUILD_SHIPPING

	static void PromptForRemoteDebugging(bool bIsEnsure)
	{
	}

	FORCEINLINE static void PrefetchBlock(const void* InPtr, int32 NumBytes = 1)
	{
	}

	/** Platform-specific instruction prefetch */
	FORCEINLINE static void Prefetch(void const* x, int32 offset = 0)
	{	
	}

	/**
	 * Gets the default profile name. Used if there is no device profile specified
	 *
	 * @return the default profile name.
	 */
	static const TCHAR* GetDefaultDeviceProfileName();
	
	/**
	 * Gets the current battery level.
	 *
	 * @return the battery level between 0 and 100.
	 */
	FORCEINLINE static int GetBatteryLevel()
	{
		return -1;
	}

	/** 
	 * Allows a game/program/etc to control the game directory in a special place (for instance, monolithic programs that don't have .uprojects)
	 */
	static void SetOverrideProjectDir(const FString& InOverrideDir);

	DEPRECATED(4.18, "FPaths::SetOverrideGameDir() has been superseded by FPaths::SetOverrideProjectDir().")
	static FORCEINLINE void SetOverrideGameDir(const FString& InOverrideDir) { return SetOverrideProjectDir(InOverrideDir); }

	/**
	 * Return an ordered list of target platforms this runtime can support (ie Android_DXT, Android
	 * would mean that it prefers Android_DXT, but can use Android as well)
	 */
	static void GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames);

	/**
	 * Returns whether the platform wants to use a touch screen for virtual joysticks.
	 */
	static bool GetUseVirtualJoysticks()
	{
		return PLATFORM_HAS_TOUCH_MAIN_SCREEN;
	}

	static bool SupportsTouchInput()
	{
		return PLATFORM_HAS_TOUCH_MAIN_SCREEN;
	}

	/*
	 * Returns whether the volume buttons are handled by the system
	 */
	static bool GetVolumeButtonsHandledBySystem()
	{
		return true;
	}

	/*
	 * Set whether the volume buttons are handled by the system
	 */
	static void SetVolumeButtonsHandledBySystem(bool enabled)
	{}

	/** @return Memory representing a true type or open type font provided by the platform as a default font for unreal to consume; empty array if the default font failed to load. */
	static TArray<uint8> GetSystemFontBytes();

	/**
	 * Returns whether WiFi connection is currently active
	 */
	static bool HasActiveWiFiConnection()
	{
		return false;
	}

	/**
	 * Returns whether the platform has variable hardware (configurable/upgradeable system).
	 */
	static bool HasVariableHardware()
	{
		// By default assume that platform hardware is variable.
		return true;
	}

	/**
	 * Returns whether the given platform feature is currently available (for instance, Metal is only available in IOS8 and with A7 devices)
	 */
	static bool HasPlatformFeature(const TCHAR* FeatureName)
	{
		return false;
	}

	/**
	 * Returns whether the platform is running on battery power or not.
	 */
	static bool IsRunningOnBattery();
	
	/**
	 * Returns the orientation of the device: e.g. Portrait, LandscapeRight.
	 * @see EScreenOrientation
	 */
	static EDeviceScreenOrientation GetDeviceOrientation();

	/**
	 * Get (or create) the unique ID used to identify this computer.
	 * This is sort of a misnomer, as there will actually be one per user account on the operating system.
	 * This is NOT based on a machine fingerprint.
	 */
	DEPRECATED(4.14, "GetMachineId is deprecated. Use GetLoginId instead.")
	static FGuid GetMachineId();

	/**
	 * Returns a unique string associated with the login account of the current machine.
	 * Implemented using persistent storage like the registry on window (using HKCU), so 
	 * is susceptible to anything that could reset or revert that storage if the ID is created,
	 * which is generally during install or first run of the app.
	 * 
	 * Note: This is NOT a user or machine fingerprint, as multiple logins on the same machine will 
	 * not share the same ID, and it is not based on the hardware of the user. It is completely random and
	 * non-identifiable.
	 *
	 * @return a string containing the LoginID, or empty if not available.
	 */
	static FString GetLoginId();

	/**
	 * Get the Epic account ID for the user who last used the Launcher.
	 * @return an empty string if the account ID was not present or it failed to read it for any reason.
	 */
	static FString GetEpicAccountId();

	/**
	 * Set the Epic account ID for the user who last used the Launcher
	 * @return true if the account ID was set successfully, false if something failed and it was not set.
	 */
	static bool SetEpicAccountId( const FString& AccountId );

	/**
	 * Gets a globally unique ID the represents a particular operating system install.
	 * @returns an opaque string representing the ID, or an empty string if the platform doesn't support one.
	 */
	static FString GetOperatingSystemId();

	/**
	 * Gets the current mode of convertible laptops, i.e. Laptop or Tablet.
	 *
	 * @return The laptop mode, or Unknown if not known, or NotSupported if not a convertible laptop.
	 */
	static EConvertibleLaptopMode GetConvertibleLaptopMode();

	/** 
	 * Get a string description of the mode the engine was running in.
	 */
	static const TCHAR* GetEngineMode();

	/**
	 * Returns an array of the user's preferred languages in order of preference
	 * @return An array of language IDs ordered from most preferred to least
	 */
	static TArray<FString> GetPreferredLanguages();

	/**
	* Returns the currency code associated with the device's locale
	* @return the currency code associated with the device's locale
	*/
	static FString GetLocalCurrencyCode();

	/**
	* Returns the currency symbol associated with the device's locale
	* @return the currency symbol associated with the device's locale
	*/
	static FString GetLocalCurrencySymbol();

	/**
	 * Requests permission to send remote notifications to the user's device.
	 */
	static void RegisterForRemoteNotifications();

	/**
	 * Returns whether or not the device has been registered to receive remote notifications.
	 */
	static bool IsRegisteredForRemoteNotifications();

	/**
	* Requests unregistering from receiving remote notifications on the user's device.
	*/
	static void UnregisterForRemoteNotifications();

	/**
	 * Allows platform at runtime to disable unsupported plugins
	 *  @param	PluginName	Name of enabled plugin to consider
	 *	@return	bool		true if plugin should be disabled
	 */
	static bool ShouldDisablePluginAtRuntime(const FString& PluginName)
	{
		return false;
	}

	/**
	 * Returns a list of platforms that are confidential in nature. To avoid hardcoding the list, this
	 * looks on disk the first time for special files, so it is non-instant.
	 */
	static const TArray<FString>& GetConfidentialPlatforms();

	
	/**
	 * Returns true if the platform allows network traffic for anonymous end user usage data
	 */
	static bool AllowSendAnonymousGameUsageDataToEpic()
	{
		return true;
	}

	/**
	 * Allows the OS to enable high DPI mode 
	 */
	static void SetHighDPIMode()
	{

	}

#if !UE_BUILD_SHIPPING
protected:
	/** Whether the user should be prompted to allow for a remote debugger to be attached */
	static bool bShouldPromptForRemoteDebugging;
	/** Whether the user should be prompted to allow for a remote debugger to be attached on an ensure */
	static bool bPromptForRemoteDebugOnEnsure;
#endif	//#if !UE_BUILD_SHIPPING
};


