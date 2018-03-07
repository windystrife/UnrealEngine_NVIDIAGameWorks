// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidMisc.h: Android platform misc functions
==============================================================================================*/

#pragma once
#include "CoreTypes.h"
#include "Android/AndroidSystemIncludes.h"
#include "GenericPlatform/GenericPlatformMisc.h"
//@todo android: this entire file

template <typename FuncType>
class TFunction;

/**
 * Android implementation of the misc OS functions
 */
struct CORE_API FAndroidMisc : public FGenericPlatformMisc
{
	static void RequestExit( bool Force );
	static void LowLevelOutputDebugString(const TCHAR *Message);
	static void LocalPrint(const TCHAR *Message);
	static void PlatformPreInit();
	static void PlatformInit();
	static void PlatformTearDown();
	static void PlatformHandleSplashScreen(bool ShowSplashScreen);
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);
	static EAppReturnType::Type MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );
	static bool AllowRenderThread();
	static bool HasPlatformFeature(const TCHAR* FeatureName);
	static bool ShouldDisablePluginAtRuntime(const FString& PluginName);
	static void SetThreadName(const char* name);
	static bool SupportsES30();

public:

	static bool AllowThreadHeartBeat()
	{
		return false;
	}

	struct FCPUStatTime{
		uint64_t			TotalTime;
		uint64_t			UserTime;
		uint64_t			NiceTime;
		uint64_t			SystemTime;
		uint64_t			SoftIRQTime;
		uint64_t			IRQTime;
		uint64_t			IdleTime;
		uint64_t			IOWaitTime;
	};

	struct FCPUState
	{
		const static int32			MaxSupportedCores = 16; //Core count 16 is maximum for now
		int32						CoreCount;
		int32						ActivatedCoreCount;
		ANSICHAR					Name[6];
		FAndroidMisc::FCPUStatTime	CurrentUsage[MaxSupportedCores]; 
		FAndroidMisc::FCPUStatTime	PreviousUsage[MaxSupportedCores];
		int32						Status[MaxSupportedCores];
		double						Utilization[MaxSupportedCores];
		double						AverageUtilization;
		
	};

	static FCPUState& GetCPUState();
	static int32 NumberOfCores();
	static bool SupportsLocalCaching();
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context));
	// NOTE: THIS FUNCTION IS DEFINED IN ANDROIDOPENGL.CPP
	static void GetValidTargetPlatforms(class TArray<class FString>& TargetPlatformNames);
	static bool GetUseVirtualJoysticks();
	static bool SupportsTouchInput();
	static const TCHAR* GetDefaultDeviceProfileName() { return TEXT("Android_Default"); }
	static bool GetVolumeButtonsHandledBySystem();
	static void SetVolumeButtonsHandledBySystem(bool enabled);
	// Returns current volume, 0-15
	static int GetVolumeState(double* OutTimeOfChangeInSec = nullptr);
	static const TCHAR* GamePersistentDownloadDir();
	static FString GetDeviceId();
	static FString GetLoginId();
	static FString GetUniqueAdvertisingId();
	static FString GetCPUVendor();
	static FString GetCPUBrand();
	static void GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel);
	static bool GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes);
	
	enum EBatteryState
	{
		BATTERY_STATE_UNKNOWN = 1,
		BATTERY_STATE_CHARGING,
		BATTERY_STATE_DISCHARGING,
		BATTERY_STATE_NOT_CHARGING,
		BATTERY_STATE_FULL
	};
	struct FBatteryState
	{
		FAndroidMisc::EBatteryState	State;
		int							Level;          // in range [0,100]
		float						Temperature;    // in degrees of Celsius
	};

	static FBatteryState GetBatteryState();
	static int GetBatteryLevel();
	static bool IsRunningOnBattery();
	static bool AreHeadPhonesPluggedIn();
	static bool HasActiveWiFiConnection();

	static void RegisterForRemoteNotifications();
	static void UnregisterForRemoteNotifications();

	/** @return Memory representing a true type or open type font provided by the platform as a default font for unreal to consume; empty array if the default font failed to load. */
	static TArray<uint8> GetSystemFontBytes();

	static IPlatformChunkInstall* GetPlatformChunkInstall();

	// ANDROID ONLY:
	static void SetVersionInfo( FString AndroidVersion, FString DeviceMake, FString DeviceModel, FString OSLanguage );
	static const FString GetAndroidVersion();
	static const FString GetDeviceMake();
	static const FString GetDeviceModel();
	static const FString GetOSLanguage();
	static FString GetDefaultLocale();
	static FString GetGPUFamily();
	static FString GetGLVersion();
	static bool SupportsFloatingPointRenderTargets();
	static bool SupportsShaderFramebufferFetch();
	static bool SupportsShaderIOBlocks();
	static int GetAndroidBuildVersion();
	static bool ShouldUseVulkan();
	static FString GetVulkanVersion();
	static bool IsDaydreamApplication();
	typedef TFunction<void(void* NewNativeHandle)> ReInitWindowCallbackType;
	static ReInitWindowCallbackType GetOnReInitWindowCallback();
	static void SetOnReInitWindowCallback(ReInitWindowCallbackType InOnReInitWindowCallback);
	static FString GetOSVersion();

#if !UE_BUILD_SHIPPING
	static bool IsDebuggerPresent();

	FORCEINLINE static void DebugBreak()
	{
		if( IsDebuggerPresent() )
		{
			__builtin_trap();
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

	FORCEINLINE static void MemoryBarrier()
	{
		__sync_synchronize();
	}


#if STATS
	/**
	* Platform specific function for adding a named event that can be viewed in PIX
	*/
	static void BeginNamedEvent(const struct FColor& Color, const TCHAR* Text);
	static void BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text);

	/**
	* Platform specific function for closing a named event that can be viewed in PIX
	*/
	static void EndNamedEvent();
#endif

#if STATS
	static int32 TraceMarkerFileDescriptor;
#endif
	
	// run time compatibility information
	static FString AndroidVersion; // version of android we are running eg "4.0.4"
	static FString DeviceMake; // make of the device we are running on eg. "samsung"
	static FString DeviceModel; // model of the device we are running on eg "SAMSUNG-SGH-I437"
	static FString OSLanguage; // language code the device is set to

	// Build version of Android, i.e. API level.
	static int32 AndroidBuildVersion;

	static bool VolumeButtonsHandledBySystem;

	enum class ECoreFrequencyProperty
	{
		CurrentFrequency,
		MaxFrequency,
		MinFrequency,
	};

	static uint32 GetCoreFrequency(int32 CoreIndex, ECoreFrequencyProperty CoreFrequencyProperty);
};

typedef FAndroidMisc FPlatformMisc;
