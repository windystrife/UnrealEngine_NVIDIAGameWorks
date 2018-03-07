// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformMisc.mm: iOS implementations of misc functions
=============================================================================*/

#include "IOSPlatformMisc.h"
#include "Misc/App.h"
#include "ExceptionHandling.h"
#include "SecureHash.h"
#include "EngineVersion.h"
#include "IOSMallocZone.h"
#include "IOSApplication.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#include "IOSChunkInstaller.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Apple/ApplePlatformCrashContext.h"
#include "IOSPlatformCrashContext.h"
#if !PLATFORM_TVOS
#include "PLCrashReporter.h"
#include "PLCrashReport.h"
#include "PLCrashReportTextFormatter.h"
#endif
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FeedbackContext.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

#if !PLATFORM_TVOS
#include <AdSupport/ASIdentifierManager.h> 
#endif

#include <SystemConfiguration/SystemConfiguration.h>
#include <netinet/in.h>

//#include <libproc.h>
// @pjs commented out to resolve issue with PLATFORM_TVOS being defined by mach-o loader
//#include <mach-o/dyld.h>

/** Amount of free memory in MB reported by the system at startup */
CORE_API int32 GStartupFreeMemoryMB;

extern CORE_API bool GIsGPUCrashed;

/** Global pointer to memory warning handler */
void (* GMemoryWarningHandler)(const FGenericMemoryWarningContext& Context) = NULL;

/** global for showing the splash screen */
bool GShowSplashScreen = true;

static int32 GetFreeMemoryMB()
{
	// get free memory
	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);

	// get memory stats
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	return (Stats.free_count * PageSize) / 1024 / 1024;
}

void FIOSPlatformMisc::PlatformInit()
{
	FAppEntry::PlatformInit();

	// Increase the maximum number of simultaneously open files
	struct rlimit Limit;
	Limit.rlim_cur = OPEN_MAX;
	Limit.rlim_max = RLIM_INFINITY;
	int32 Result = setrlimit(RLIMIT_NOFILE, &Limit);
	check(Result == 0);

	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName() );
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName() );

	
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("CPU Page size=%i, Cores=%i"), MemoryConstants.PageSize, FPlatformMisc::NumberOfCores() );

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle() );
	GStartupFreeMemoryMB = GetFreeMemoryMB();
	UE_LOG(LogInit, Log, TEXT("Free Memory at startup: %d MB"), GStartupFreeMemoryMB);
}

void FIOSPlatformMisc::PlatformHandleSplashScreen(bool ShowSplashScreen)
{
    GShowSplashScreen = ShowSplashScreen;
}

EAppReturnType::Type FIOSPlatformMisc::MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
	extern EAppReturnType::Type MessageBoxExtImpl( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );
	return MessageBoxExtImpl(MsgType, Text, Caption);
}

int FIOSPlatformMisc::GetAudioVolume()
{
	return [[IOSAppDelegate GetDelegate] GetAudioVolume];
}

bool FIOSPlatformMisc::AreHeadphonesPluggedIn()
{
	return [[IOSAppDelegate GetDelegate] AreHeadphonesPluggedIn];
}

int FIOSPlatformMisc::GetBatteryLevel()
{
	return [[IOSAppDelegate GetDelegate] GetBatteryLevel];
}

bool FIOSPlatformMisc::IsRunningOnBattery()
{
	return [[IOSAppDelegate GetDelegate] IsRunningOnBattery];
}

#if !PLATFORM_TVOS
EDeviceScreenOrientation ConvertFromUIDeviceOrientation(UIDeviceOrientation Orientation)
{
	switch(Orientation)
	{
		default:
		case UIDeviceOrientationUnknown : return EDeviceScreenOrientation::Unknown; break;
		case UIDeviceOrientationPortrait : return EDeviceScreenOrientation::Portrait; break;
		case UIDeviceOrientationPortraitUpsideDown : return EDeviceScreenOrientation::PortraitUpsideDown; break;
		case UIDeviceOrientationLandscapeLeft : return EDeviceScreenOrientation::LandscapeLeft; break;
		case UIDeviceOrientationLandscapeRight : return EDeviceScreenOrientation::LandscapeRight; break;
		case UIDeviceOrientationFaceUp : return EDeviceScreenOrientation::FaceUp; break;
		case UIDeviceOrientationFaceDown : return EDeviceScreenOrientation::FaceDown; break;
	}
}
#endif

EDeviceScreenOrientation FIOSPlatformMisc::GetDeviceOrientation()
{
#if !PLATFORM_TVOS
	return ConvertFromUIDeviceOrientation([[UIDevice currentDevice] orientation]);
#else
	return EDeviceScreenOrientation::Unknown;
#endif
}

#include "ModuleManager.h"

bool FIOSPlatformMisc::HasPlatformFeature(const TCHAR* FeatureName)
{
	if (FCString::Stricmp(FeatureName, TEXT("Metal")) == 0)
	{
		return [IOSAppDelegate GetDelegate].IOSView->bIsUsingMetal;
	}

	return FGenericPlatformMisc::HasPlatformFeature(FeatureName);
}

FString GetIOSDeviceIDString()
{
	static FString CachedResult;
	static bool bCached = false;
	if (!bCached)
	{
		// get the device hardware type string length
		size_t DeviceIDLen;
		sysctlbyname("hw.machine", NULL, &DeviceIDLen, NULL, 0);

		// get the device hardware type
		char* DeviceID = (char*)malloc(DeviceIDLen);
		sysctlbyname("hw.machine", DeviceID, &DeviceIDLen, NULL, 0);

		CachedResult = ANSI_TO_TCHAR(DeviceID);
		bCached = true;

		free(DeviceID);
	}

	return CachedResult;
}

FIOSPlatformMisc::EIOSDevice FIOSPlatformMisc::GetIOSDeviceType()
{
	// default to unknown
	static EIOSDevice DeviceType = IOS_Unknown;

	// if we've already figured it out, return it
	if (DeviceType != IOS_Unknown)
	{
		return DeviceType;
	}

	const FString DeviceIDString = GetIOSDeviceIDString();

	// iPods
	if (DeviceIDString.StartsWith(TEXT("iPod")))
	{
		// get major revision number
        int Major = FCString::Atoi(&DeviceIDString[4]);

		if (Major == 5)
		{
			DeviceType = IOS_IPodTouch5;
		}
		else if (Major >= 7)
		{
			DeviceType = IOS_IPodTouch6;
		}
	}
	// iPads
	else if (DeviceIDString.StartsWith(TEXT("iPad")))
	{
		// get major revision number
		const int Major = FCString::Atoi(&DeviceIDString[4]);
		const int CommaIndex = DeviceIDString.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, 4);
		const int Minor = FCString::Atoi(&DeviceIDString[CommaIndex + 1]);

		// iPad2,[1|2|3] is iPad 2 (1 - wifi, 2 - gsm, 3 - cdma)
		if (Major == 2)
		{
			// iPad2,5+ is the new iPadMini, anything higher will use these settings until released
			if (Minor >= 5)
			{
				DeviceType = IOS_IPadMini;
			}
			else
			{
				DeviceType = IOS_IPad2;
			}
		}
		// iPad3,[1|2|3] is iPad 3 and iPad3,4+ is iPad (4th generation)
		else if (Major == 3)
		{
			if (Minor <= 3)
			{
				DeviceType = IOS_IPad3;
			}
			// iPad3,4+ is the new iPad, anything higher will use these settings until released
			else if (Minor >= 4)
			{
				DeviceType = IOS_IPad4;
			}
		}
		// iPadAir and iPad Mini 2nd Generation
		else if (Major == 4)
		{
			if (Minor >= 4)
			{
				DeviceType = IOS_IPadMini2;
			}
			else
			{
				DeviceType = IOS_IPadAir;
			}
		}
		// iPad Air 2 and iPadMini 4
		else if (Major == 5)
		{
			if (Minor == 1 || Minor == 2)
			{
				DeviceType = IOS_IPadMini4;
			}
			else
			{
				DeviceType = IOS_IPadAir2;
			}
		}
		else if (Major == 6)
		{
			if (Minor == 3 || Minor == 4)
			{
				DeviceType = IOS_IPadPro_97;
			}
			else if (Minor == 11 || Minor == 12)
			{
				DeviceType = IOS_IPad5;
			}
			else
			{
				DeviceType = IOS_IPadPro_129;
			}
		}
		else if (Major == 7)
		{
			if (Minor == 3 || Minor == 4)
			{
				DeviceType = IOS_IPadPro_105;
			}
			else
			{
				DeviceType = IOS_IPadPro2_129;
			}
		}

		// Default to highest settings currently available for any future device
		else if (Major > 6)
		{
			DeviceType = IOS_IPadPro;
		}
	}
	// iPhones
	else if (DeviceIDString.StartsWith(TEXT("iPhone")))
	{
        const int Major = FCString::Atoi(&DeviceIDString[6]);
		const int CommaIndex = DeviceIDString.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, 6);
		const int Minor = FCString::Atoi(&DeviceIDString[CommaIndex + 1]);

		if (Major == 3)
		{
			DeviceType = IOS_IPhone4;
		}
		else if (Major == 4)
		{
			DeviceType = IOS_IPhone4S;
		}
		else if (Major == 5)
		{
			DeviceType = IOS_IPhone5;
		}
		else if (Major == 6)
		{
			DeviceType = IOS_IPhone5S;
		}
		else if (Major == 7)
		{
			if (Minor == 1)
			{
				DeviceType = IOS_IPhone6Plus;
			}
			else if (Minor == 2)
			{
				DeviceType = IOS_IPhone6;
			}
		}
		else if (Major == 8)
		{
			// note that Apple switched the minor order around between 6 and 6S (gotta keep us on our toes!)
			if (Minor == 1)
			{
				DeviceType = IOS_IPhone6S;
			}
			else if (Minor == 2)
			{
				DeviceType = IOS_IPhone6SPlus;
			}
			else if (Minor == 4)
			{
				DeviceType = IOS_IPhoneSE;
			}
		}
		else if (Major == 9)
		{
            if (Minor == 1 || Minor == 3)
            {
                DeviceType = IOS_IPhone7;
            }
            else if (Minor == 2 || Minor == 4)
            {
                DeviceType = IOS_IPhone7Plus;
            }
		}
        else if (Major == 10)
        {
			if (Minor == 1 || Minor == 4)
			{
				DeviceType = IOS_IPhone8;
			}
			else if (Minor == 2 || Minor == 5)
			{
				DeviceType = IOS_IPhone8Plus;
			}
			else if (Minor == 3 || Minor == 6)
			{
				DeviceType = IOS_IPhoneX;
			}
		}
		else if (Major >= 10)
		{
			// for going forward into unknown devices (like 8/8+?), we can't use Minor,
			// so treat devices with a scale > 2.5 to be 6SPlus type devices, < 2.5 to be 6S type devices
			if ([UIScreen mainScreen].scale > 2.5f)
			{
				DeviceType = IOS_IPhone8Plus;
			}
			else
			{
				DeviceType = IOS_IPhone8;
			}
		}
	}
	// tvOS
	else if (DeviceIDString.StartsWith(TEXT("AppleTV")))
	{
		const int Major = FCString::Atoi(&DeviceIDString[7]);
		const int CommaIndex = DeviceIDString.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, 6);
		const int Minor = FCString::Atoi(&DeviceIDString[CommaIndex + 1]);

		if (Major == 5)
		{
			DeviceType = IOS_AppleTV;
		}
		else if (Major == 6)
		{
			DeviceType = IOS_AppleTV4K;
		}
		else if (Major >= 6)
		{
			DeviceType = IOS_AppleTV4K;
		}
	}
	// simulator
	else if (DeviceIDString.StartsWith(TEXT("x86")))
	{
		// iphone
		if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
		{
			CGSize result = [[UIScreen mainScreen] bounds].size;
			if(result.height >= 586)
			{
				DeviceType = IOS_IPhone5;
			}
			else
			{
				DeviceType = IOS_IPhone4S;
			}
		}
		else
		{
			if ([[UIScreen mainScreen] scale] > 1.0f)
			{
				DeviceType = IOS_IPad4;
			}
			else
			{
				DeviceType = IOS_IPad2;
			}
		}
	}

	// if this is unknown at this point, we have a problem
	if (DeviceType == IOS_Unknown)
	{
		UE_LOG(LogInit, Fatal, TEXT("This IOS device type is not supported by UE4 [%s]\n"), *FString(DeviceIDString));
	}

	return DeviceType;
}

int FIOSPlatformMisc::GetDefaultStackSize()
{
	return 4 * 1024 * 1024;
}

void FIOSPlatformMisc::SetMemoryWarningHandler(void (* InHandler)(const FGenericMemoryWarningContext& Context))
{
	GMemoryWarningHandler = InHandler;
}

void FIOSPlatformMisc::HandleLowMemoryWarning()
{
	UE_LOG(LogInit, Log, TEXT("Free Memory at Startup: %d MB"), GStartupFreeMemoryMB);
	UE_LOG(LogInit, Log, TEXT("Free Memory Now       : %d MB"), GetFreeMemoryMB());

	if(GMemoryWarningHandler != NULL)
	{
		FGenericMemoryWarningContext Context;
		GMemoryWarningHandler(Context);
	}
}

bool FIOSPlatformMisc::IsPackagedForDistribution()
{
#if !UE_BUILD_SHIPPING
	static bool PackagingModeCmdLine = FParse::Param(FCommandLine::Get(), TEXT("PACKAGED_FOR_DISTRIBUTION"));
	if (PackagingModeCmdLine)
	{
		return true;
	}
#endif
	NSString* PackagingMode = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"EpicPackagingMode"];
	return PackagingMode != nil && [PackagingMode isEqualToString : @"Distribution"];
}

/**
 * Returns a unique string for device identification
 * 
 * @return the unique string generated by this platform for this device
 */
FString FIOSPlatformMisc::GetUniqueDeviceId()
{
	// Check to see if this OS has this function
	if ([[UIDevice currentDevice] respondsToSelector:@selector(identifierForVendor)])
	{
		NSUUID* Id = [[UIDevice currentDevice] identifierForVendor];
		if (Id != nil)
		{
			return FString([[Id UUIDString] autorelease]);
		}
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return FPlatformMisc::GetHashedMacAddressString();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FString FIOSPlatformMisc::GetDeviceId()
{
	// Check to see if this OS has this function

	if ([[UIDevice currentDevice] respondsToSelector:@selector(identifierForVendor)])
	{
	    NSUUID* Id = [[UIDevice currentDevice] identifierForVendor];
	    if (Id != nil)
	    {
		    NSString* IdfvString = [Id UUIDString];
		    FString IDFV(IdfvString);
		    return IDFV;
	    }
	}
	return FString();
}

FString FIOSPlatformMisc::GetOSVersion()
{
	return FString([[UIDevice currentDevice] systemVersion]);
}

bool FIOSPlatformMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
	NSDictionary<NSFileAttributeKey, id>* FSStat = [[NSFileManager defaultManager] attributesOfFileSystemForPath:NSHomeDirectory() error:nil];
	if (FSStat)
	{
		TotalNumberOfBytes = [[FSStat objectForKey:NSFileSystemSize] longLongValue];
		NumberOfFreeBytes = [[FSStat objectForKey:NSFileSystemFreeSize] longLongValue];
		return true;
	}
	return false;
}


/**
* Returns a unique string for advertising identification
*
* @return the unique string generated by this platform for this device
*/
FString FIOSPlatformMisc::GetUniqueAdvertisingId()
{
#if !PLATFORM_TVOS
	// Check to see if this OS has this function
	if ([[ASIdentifierManager sharedManager] respondsToSelector:@selector(advertisingIdentifier)])
	{
		NSString* IdfaString = [[[ASIdentifierManager sharedManager] advertisingIdentifier] UUIDString];
		FString IDFA(IdfaString);
		return IDFA;
	}
#endif
	return FString();
}

class IPlatformChunkInstall* FIOSPlatformMisc::GetPlatformChunkInstall()
{
	static IPlatformChunkInstall* ChunkInstall = nullptr;
	static bool bIniChecked = false;
	if (!ChunkInstall || !bIniChecked)
	{
		FString ProviderName;
		IPlatformChunkInstallModule* PlatformChunkInstallModule = nullptr;
		if (!GEngineIni.IsEmpty())
		{
			FString InstallModule;
			GConfig->GetString(TEXT("StreamingInstall"), TEXT("DefaultProviderName"), InstallModule, GEngineIni);
			FModuleStatus Status;
			if (FModuleManager::Get().QueryModule(*InstallModule, Status))
			{
				PlatformChunkInstallModule = FModuleManager::LoadModulePtr<IPlatformChunkInstallModule>(*InstallModule);
				if (PlatformChunkInstallModule != nullptr)
				{
					// Attempt to grab the platform installer
					ChunkInstall = PlatformChunkInstallModule->GetPlatformChunkInstall();
				}
			}
			else if (ProviderName == TEXT("IOSChunkInstaller"))
			{
				static FIOSChunkInstall Singleton;
				ChunkInstall = &Singleton;
			}
			bIniChecked = true;
		}
		if (!ChunkInstall)
		{
			// Placeholder instance
			ChunkInstall = FGenericPlatformMisc::GetPlatformChunkInstall();
		}
	}

	return ChunkInstall;
}

void FIOSPlatformMisc::RegisterForRemoteNotifications()
{
#if !PLATFORM_TVOS && NOTIFICATIONS_ENABLED
	UIApplication* application = [UIApplication sharedApplication];
	if ([application respondsToSelector : @selector(registerUserNotificationSettings:)])
	{
#ifdef __IPHONE_8_0
		UIUserNotificationSettings * settings = [UIUserNotificationSettings settingsForTypes : (UIUserNotificationTypeBadge | UIUserNotificationTypeSound | UIUserNotificationTypeAlert) categories:nil];
		[application registerUserNotificationSettings : settings];
#endif
	}
	else
	{
        
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_8_0
		UIRemoteNotificationType myTypes = UIRemoteNotificationTypeBadge | UIRemoteNotificationTypeAlert | UIRemoteNotificationTypeSound;
		[application registerForRemoteNotificationTypes : myTypes];
#endif
	}
#endif
}

bool FIOSPlatformMisc::IsRegisteredForRemoteNotifications()
{
	return false;
}

void FIOSPlatformMisc::UnregisterForRemoteNotifications()
{

}

void FIOSPlatformMisc::GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames)
{
	// this is only used to cook with the proper TargetPlatform with COTF, it's not the runtime platform (which is just IOS for both)
#if PLATFORM_TVOS
	TargetPlatformNames.Add(TEXT("TVOS"));
#else
	TargetPlatformNames.Add(FIOSPlatformProperties::PlatformName());
#endif
}

bool FIOSPlatformMisc::HasActiveWiFiConnection()
{
	struct sockaddr_in ZeroAddress;
	FMemory::Memzero(&ZeroAddress, sizeof(ZeroAddress));
	ZeroAddress.sin_len = sizeof(ZeroAddress);
	ZeroAddress.sin_family = AF_INET;
	
	SCNetworkReachabilityRef ReachabilityRef = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, (const struct sockaddr*)&ZeroAddress);
	SCNetworkReachabilityFlags ReachabilityFlags;
	bool bFlagsAvailable = SCNetworkReachabilityGetFlags(ReachabilityRef, &ReachabilityFlags);
	CFRelease(ReachabilityRef);
	
	bool bHasActiveWiFiConnection = false;
	if (bFlagsAvailable)
	{
		bool bReachable =	(ReachabilityFlags & kSCNetworkReachabilityFlagsReachable) != 0 &&
		(ReachabilityFlags & kSCNetworkReachabilityFlagsConnectionRequired) == 0 &&
		// in case kSCNetworkReachabilityFlagsConnectionOnDemand  || kSCNetworkReachabilityFlagsConnectionOnTraffic
		(ReachabilityFlags & kSCNetworkReachabilityFlagsInterventionRequired) == 0;
		
		bHasActiveWiFiConnection = bReachable && (ReachabilityFlags & kSCNetworkReachabilityFlagsIsWWAN) == 0;
	}
	
	return bHasActiveWiFiConnection;
}

FString FIOSPlatformMisc::GetCPUVendor()
{
	return TEXT("Apple");
}

FString FIOSPlatformMisc::GetCPUBrand()
{
	return GetIOSDeviceIDString();
}

void FIOSPlatformMisc::GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel)
{
#if PLATFORM_TVOS
	out_OSVersionLabel = TEXT("TVOS");
#else
	out_OSVersionLabel = TEXT("IOS");
#endif
	NSOperatingSystemVersion IOSVersion;
	IOSVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
	out_OSSubVersionLabel = FString::Printf(TEXT("%ld.%ld.%ld"), IOSVersion.majorVersion, IOSVersion.minorVersion, IOSVersion.patchVersion);
}

int32 FIOSPlatformMisc::IOSVersionCompare(uint8 Major, uint8 Minor, uint8 Revision)
{
	NSOperatingSystemVersion IOSVersion;
	IOSVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
	uint8 TargetValues[3] = { Major, Minor, Revision };
	NSInteger ComponentValues[3] = { IOSVersion.majorVersion, IOSVersion.minorVersion, IOSVersion.patchVersion };

	for (uint32 i = 0; i < 3; i++)
	{
		if (ComponentValues[i] < TargetValues[i])
		{
			return -1;
		}
		else if (ComponentValues[i] > TargetValues[i])
		{
			return 1;
		}
	}

	return 0;
}

/*------------------------------------------------------------------------------
 FIOSApplicationInfo - class to contain all state for crash reporting that is unsafe to acquire in a signal.
 ------------------------------------------------------------------------------*/

/**
 * Information that cannot be obtained during a signal-handler is initialised here.
 * This ensures that we only call safe functions within the crash reporting handler.
 */
struct FIOSApplicationInfo
{
    void Init()
    {
        SCOPED_AUTORELEASE_POOL;
        
        AppName = FApp::GetProjectName();
        FCStringAnsi::Strcpy(AppNameUTF8, PATH_MAX+1, TCHAR_TO_UTF8(*AppName));
        
        ExecutableName = FPlatformProcess::ExecutableName();
        
        AppPath = FString([[NSBundle mainBundle] executablePath]);
        
        AppBundleID = FString([[NSBundle mainBundle] bundleIdentifier]);
        
        NumCores = FPlatformMisc::NumberOfCores();
        
        LCID = FString::Printf(TEXT("%d"), FInternationalization::Get().GetCurrentCulture()->GetLCID());
        
        PrimaryGPU = FPlatformMisc::GetPrimaryGPUBrand();
        
        RunUUID = RunGUID();
        
        OSXVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
        OSVersion = FString::Printf(TEXT("%ld.%ld.%ld"), OSXVersion.majorVersion, OSXVersion.minorVersion, OSXVersion.patchVersion);
        FCStringAnsi::Strcpy(OSVersionUTF8, PATH_MAX+1, TCHAR_TO_UTF8(*OSVersion));
        
        // macOS build number is only accessible on non-sandboxed applications as it resides outside the accessible sandbox
        if(!bIsSandboxed)
        {
            NSDictionary* SystemVersion = [NSDictionary dictionaryWithContentsOfFile: @"/System/Library/CoreServices/SystemVersion.plist"];
            OSBuild = FString((NSString*)[SystemVersion objectForKey: @"ProductBuildVersion"]);
        }
        
        char TempSysCtlBuffer[PATH_MAX] = {};
        size_t TempSysCtlBufferSize = PATH_MAX;
        
        sysctlbyname("kern.osrelease", TempSysCtlBuffer, &TempSysCtlBufferSize, NULL, 0);
        BiosRelease = TempSysCtlBuffer;
        uint32 KernelRevision = 0;
        TempSysCtlBufferSize = 4;
        sysctlbyname("kern.osrevision", &KernelRevision, &TempSysCtlBufferSize, NULL, 0);
        BiosRevision = FString::Printf(TEXT("%d"), KernelRevision);
        TempSysCtlBufferSize = PATH_MAX;
        sysctlbyname("kern.uuid", TempSysCtlBuffer, &TempSysCtlBufferSize, NULL, 0);
        BiosUUID = TempSysCtlBuffer;
        TempSysCtlBufferSize = PATH_MAX;
        sysctlbyname("hw.model", TempSysCtlBuffer, &TempSysCtlBufferSize, NULL, 0);
        MachineModel = TempSysCtlBuffer;
        TempSysCtlBufferSize = PATH_MAX+1;
        sysctlbyname("machdep.cpu.brand_string", MachineCPUString, &TempSysCtlBufferSize, NULL, 0);
        
        gethostname(MachineName, ARRAY_COUNT(MachineName));
        
       BranchBaseDir = FString::Printf( TEXT( "%s!%s!%s!%d" ), *FApp::GetBranchName(), FPlatformProcess::BaseDir(), FPlatformMisc::GetEngineMode(), FEngineVersion::Current().GetChangelist() );
        
        // Get the paths that the files will actually have been saved to
        FString LogDirectory = FPaths::ProjectLogDir();
        TCHAR CommandlineLogFile[MAX_SPRINTF]=TEXT("");
        
        // Use the log file specified on the commandline if there is one
        CommandLine = FCommandLine::Get();
        FString LogPath = FGenericPlatformOutputDevices::GetAbsoluteLogFilename();
        LogPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*LogPath);
        FCStringAnsi::Strcpy(AppLogPath, PATH_MAX + 1, TCHAR_TO_UTF8(*LogPath));
        
        // Cache & create the crash report folder.
        FString ReportPath = FPaths::ConvertRelativePathToFull(FString::Printf(TEXT("%s"), *(FPaths::GameAgnosticSavedDir() / TEXT("Crashes"))));
        IFileManager::Get().MakeDirectory(*ReportPath, true);
        ReportPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ReportPath);
        FCStringAnsi::Strcpy(CrashReportPath, PATH_MAX+1, TCHAR_TO_UTF8(*ReportPath));
        
        NSString* PLCrashReportFile = [TemporaryCrashReportFolder().GetNSString() stringByAppendingPathComponent:TemporaryCrashReportName().GetNSString()];
        [PLCrashReportFile getCString:PLCrashReportPath maxLength:PATH_MAX encoding:NSUTF8StringEncoding];
    }
    
    ~FIOSApplicationInfo()
    {
#if !PLATFORM_TVOS
        if (CrashReporter)
        {
            [CrashReporter release];
            CrashReporter = nil;
        }
#endif
    }
    
    static FGuid RunGUID()
    {
        static FGuid Guid;
        if (!Guid.IsValid())
        {
            FPlatformMisc::CreateGuid(Guid);
        }
        return Guid;
    }
    
    static FString TemporaryCrashReportFolder()
    {
        static FString PLCrashReportFolder;
        if (PLCrashReportFolder.IsEmpty())
        {
            SCOPED_AUTORELEASE_POOL;
            
            NSArray* Paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
            NSString* CacheDir = [Paths objectAtIndex: 0];
            
            NSString* BundleID = [[NSBundle mainBundle] bundleIdentifier];
            if (!BundleID)
            {
                BundleID = [[NSProcessInfo processInfo] processName];
            }
            check(BundleID);
            
            NSString* PLCrashReportFolderPath = [CacheDir stringByAppendingPathComponent: BundleID];
            PLCrashReportFolder = FString(PLCrashReportFolderPath);
        }
        return PLCrashReportFolder;
    }
    
    static FString TemporaryCrashReportName()
    {
        static FString PLCrashReportFileName(RunGUID().ToString() + TEXT(".plcrash"));
        return PLCrashReportFileName;
    }
    
    bool bIsSandboxed;
    int32 NumCores;
    char AppNameUTF8[PATH_MAX+1];
    char AppLogPath[PATH_MAX+1];
    char CrashReportPath[PATH_MAX+1];
    char PLCrashReportPath[PATH_MAX+1];
    char OSVersionUTF8[PATH_MAX+1];
    char MachineName[PATH_MAX+1];
    char MachineCPUString[PATH_MAX+1];
    FString AppPath;
    FString AppName;
    FString AppBundleID;
    FString OSVersion;
    FString OSBuild;
    FString MachineUUID;
    FString MachineModel;
    FString BiosRelease;
    FString BiosRevision;
    FString BiosUUID;
    FString ParentProcess;
    FString LCID;
    FString CommandLine;
    FString BranchBaseDir;
    FString PrimaryGPU;
    FString ExecutableName;
    NSOperatingSystemVersion OSXVersion;
    FGuid RunUUID;
    FString XcodePath;
#if !PLATFORM_TVOS
    static PLCrashReporter* CrashReporter;
#endif
    static FIOSMallocCrashHandler* CrashMalloc;
};

static FIOSApplicationInfo GIOSAppInfo;
#if !PLATFORM_TVOS
PLCrashReporter* FIOSApplicationInfo::CrashReporter = nullptr;
#endif
FIOSMallocCrashHandler* FIOSApplicationInfo::CrashMalloc = nullptr;

void (*GCrashHandlerPointer)(const FGenericCrashContext& Context) = NULL;

// good enough default crash reporter
static void DefaultCrashHandler(FIOSCrashContext const& Context)
{
    Context.ReportCrash();
    if (GLog)
    {
        GLog->SetCurrentThreadAsMasterThread();
        GLog->Flush();
    }
    if (GWarn)
    {
        GWarn->Flush();
    }
    if (GError)
    {
        GError->Flush();
        GError->HandleError();
    }
    return Context.GenerateCrashInfo();
}

// number of stack entries to ignore in backtrace
static uint32 GIOSStackIgnoreDepth = 6;

// true system specific crash handler that gets called first
static void PlatformCrashHandler(int32 Signal, siginfo_t* Info, void* Context)
{
    FIOSCrashContext CrashContext;
    CrashContext.IgnoreDepth = GIOSStackIgnoreDepth;
    CrashContext.InitFromSignal(Signal, Info, Context);
    
    // switch to crash handler malloc to avoid malloc reentrancy
    check(FIOSApplicationInfo::CrashMalloc);
    FIOSApplicationInfo::CrashMalloc->Enable(&CrashContext, FPlatformTLS::GetCurrentThreadId());
    
    if (GCrashHandlerPointer)
    {
        GCrashHandlerPointer(CrashContext);
    }
    else
    {
        // call default one
        DefaultCrashHandler(CrashContext);
    }
}

static void PLCrashReporterHandler(siginfo_t* Info, ucontext_t* Uap, void* Context)
{
    PlatformCrashHandler((int32)Info->si_signo, Info, Uap);
}

// handles graceful termination.
static void GracefulTerminationHandler(int32 Signal, siginfo_t* Info, void* Context)
{
    // make sure we write out as much as possible
    if (GLog)
    {
        GLog->Flush();
    }
    if (GWarn)
    {
        GWarn->Flush();
    }
    if (GError)
    {
        GError->Flush();
    }
    
    if (!GIsRequestingExit)
    {
        GIsRequestingExit = 1;
    }
    else
    {
        _Exit(0);
    }
}

void FIOSPlatformMisc::PlatformPreInit()
{
    FGenericPlatformMisc::PlatformPreInit();
    
    GIOSAppInfo.Init();
    
    // turn off SIGPIPE crashes
    signal(SIGPIPE, SIG_IGN);
}

// Make sure that SetStoredValue and GetStoredValue generate the same key
static NSString* MakeStoredValueKeyName(const FString& SectionName, const FString& KeyName)
{
	return [NSString stringWithFString:(SectionName + "/" + KeyName)];
}

bool FIOSPlatformMisc::SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];

	// convert input to an NSString
	NSString* StoredValue = [NSString stringWithFString:InValue];

	// store it
	[UserSettings setObject:StoredValue forKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	return true;
}

bool FIOSPlatformMisc::GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];

	// get the stored NSString
	NSString* StoredValue = [UserSettings objectForKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	// if it was there, convert back to FString
	if (StoredValue != nil)
	{
		OutValue = StoredValue;
		return true;
	}

	return false;
}

bool FIOSPlatformMisc::DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName)
{
	// No Implementation (currently only used by editor code so not needed on iOS)
	return false;
}

void FIOSPlatformMisc::SetGracefulTerminationHandler()
{
    struct sigaction Action;
    FMemory::Memzero(&Action, sizeof(struct sigaction));
    Action.sa_sigaction = GracefulTerminationHandler;
    sigemptyset(&Action.sa_mask);
    Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
    sigaction(SIGINT, &Action, NULL);
    sigaction(SIGTERM, &Action, NULL);
    sigaction(SIGHUP, &Action, NULL);
}

void FIOSPlatformMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context))
{
    SCOPED_AUTORELEASE_POOL;
    
    GCrashHandlerPointer = CrashHandler;
    
#if !PLATFORM_TVOS
    if (!FIOSApplicationInfo::CrashReporter && !FIOSApplicationInfo::CrashMalloc)
    {
        // configure the crash handler malloc zone to reserve a little memory for itself
        FIOSApplicationInfo::CrashMalloc = new FIOSMallocCrashHandler(128*1024);
        
        PLCrashReporterConfig* Config = [[[PLCrashReporterConfig alloc] initWithSignalHandlerType: PLCrashReporterSignalHandlerTypeBSD symbolicationStrategy: PLCrashReporterSymbolicationStrategyNone crashReportFolder: FIOSApplicationInfo::TemporaryCrashReportFolder().GetNSString() crashReportName: FIOSApplicationInfo::TemporaryCrashReportName().GetNSString()] autorelease];
        FIOSApplicationInfo::CrashReporter = [[PLCrashReporter alloc] initWithConfiguration: Config];
        
        PLCrashReporterCallbacks CrashReportCallback = {
            .version = 0,
            .context = nullptr,
            .handleSignal = PLCrashReporterHandler
        };
        
        [FIOSApplicationInfo::CrashReporter setCrashCallbacks: &CrashReportCallback];
        
        NSError* Error = nil;
        if ([FIOSApplicationInfo::CrashReporter enableCrashReporterAndReturnError: &Error])
        {
            GIOSStackIgnoreDepth = 0;
        }
        else
        {
            UE_LOG(LogIOS, Log, TEXT("Failed to enable PLCrashReporter: %s"), *FString([Error localizedDescription]));
            UE_LOG(LogIOS, Log, TEXT("Falling back to native signal handlers"));
 
            struct sigaction Action;
            FMemory::Memzero(&Action, sizeof(struct sigaction));
            Action.sa_sigaction = PlatformCrashHandler;
            sigemptyset(&Action.sa_mask);
            Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
            sigaction(SIGQUIT, &Action, NULL);
            sigaction(SIGILL, &Action, NULL);
            sigaction(SIGEMT, &Action, NULL);
            sigaction(SIGFPE, &Action, NULL);
            sigaction(SIGBUS, &Action, NULL);
            sigaction(SIGSEGV, &Action, NULL);
            sigaction(SIGSYS, &Action, NULL);
            sigaction(SIGABRT, &Action, NULL);
        }
    }
#endif
}

void FIOSCrashContext::GenerateWindowsErrorReport(char const* WERPath, bool bIsEnsure) const
{
	// @pjs commented out to resolve issue with PLATFORM_TVOS being defined by mach-o loader
/*    int ReportFile = open(WERPath, O_CREAT|O_WRONLY, 0766);
    if (ReportFile != -1)
    {
        TCHAR Line[PATH_MAX] = {};
        
        // write BOM
        static uint16 ByteOrderMarker = 0xFEFF;
        write(ReportFile, &ByteOrderMarker, sizeof(ByteOrderMarker));
        
        WriteLine(ReportFile, TEXT("<?xml version=\"1.0\" encoding=\"UTF-16\"?>"));
        WriteLine(ReportFile, TEXT("<WERReportMetadata>"));
        
        WriteLine(ReportFile, TEXT("\t<OSVersionInformation>"));
        WriteUTF16String(ReportFile, TEXT("\t\t<WindowsNTVersion>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.OSVersion);
        WriteLine(ReportFile, TEXT("</WindowsNTVersion>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Build>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.OSVersion);
        WriteUTF16String(ReportFile, TEXT(" ("));
        WriteUTF16String(ReportFile, *GIOSAppInfo.OSBuild);
        WriteLine(ReportFile, TEXT(")</Build>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Product>(0x30): IOS "));
        WriteUTF16String(ReportFile, *GIOSAppInfo.OSVersion);
        WriteLine(ReportFile, TEXT("</Product>"));
        
        WriteLine(ReportFile, TEXT("\t\t<Edition>IOS</Edition>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<BuildString>IOS "));
        WriteUTF16String(ReportFile, *GIOSAppInfo.OSVersion);
        WriteUTF16String(ReportFile, TEXT(" ("));
        WriteUTF16String(ReportFile, *GIOSAppInfo.OSBuild);
        WriteLine(ReportFile, TEXT(")</BuildString>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Revision>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.OSBuild);
        WriteLine(ReportFile, TEXT("</Revision>"));
        
        WriteLine(ReportFile, TEXT("\t\t<Flavor>Multiprocessor Free</Flavor>"));
        WriteLine(ReportFile, TEXT("\t\t<Architecture>X64</Architecture>"));
        WriteUTF16String(ReportFile, TEXT("\t\t<LCID>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.LCID);
        WriteLine(ReportFile, TEXT("</LCID>"));
        WriteLine(ReportFile, TEXT("\t</OSVersionInformation>"));
        
        WriteLine(ReportFile, TEXT("\t<ParentProcessInformation>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<ParentProcessId>"));
        WriteUTF16String(ReportFile, ItoTCHAR(getppid(), 10));
        WriteLine(ReportFile, TEXT("</ParentProcessId>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<ParentProcessPath>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.ParentProcess);
        WriteLine(ReportFile, TEXT("</ParentProcessPath>"));
        
        WriteLine(ReportFile, TEXT("\t\t<ParentProcessCmdLine></ParentProcessCmdLine>"));	// FIXME: supply valid?
        WriteLine(ReportFile, TEXT("\t</ParentProcessInformation>"));
        
        WriteLine(ReportFile, TEXT("\t<ProblemSignatures>"));
        WriteLine(ReportFile, TEXT("\t\t<EventType>APPCRASH</EventType>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Parameter0>UE4-"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.AppName);
        WriteLine(ReportFile, TEXT("</Parameter0>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Parameter1>"));
        WriteUTF16String(ReportFile, ItoTCHAR(FEngineVersion::Current().GetMajor(), 10));
        WriteUTF16String(ReportFile, TEXT("."));
        WriteUTF16String(ReportFile, ItoTCHAR(FEngineVersion::Current().GetMinor(), 10));
        WriteUTF16String(ReportFile, TEXT("."));
        WriteUTF16String(ReportFile, ItoTCHAR(FEngineVersion::Current().GetPatch(), 10));
        WriteLine(ReportFile, TEXT("</Parameter1>"));
        
        // App time stamp
        WriteLine(ReportFile, TEXT("\t\t<Parameter2>528f2d37</Parameter2>"));													// FIXME: supply valid?
        
        Dl_info DLInfo;
        if(Info && Info->si_addr != 0 && dladdr(Info->si_addr, &DLInfo) != 0)
        {
            // Crash Module name
            WriteUTF16String(ReportFile, TEXT("\t\t<Parameter3>"));
            if (DLInfo.dli_fname && FCStringAnsi::Strlen(DLInfo.dli_fname))
            {
                FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
                FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, DLInfo.dli_fname, FCStringAnsi::Strlen(DLInfo.dli_fname));
                WriteUTF16String(ReportFile, Line);
            }
            else
            {
                WriteUTF16String(ReportFile, TEXT("Unknown"));
            }
            WriteLine(ReportFile, TEXT("</Parameter3>"));
            
            // Check header
            uint32 Version = 0;
            uint32 TimeStamp = 0;
            struct mach_header_64* Header = (struct mach_header_64*)DLInfo.dli_fbase;
            struct load_command *CurrentCommand = (struct load_command *)( (char *)Header + sizeof(struct mach_header_64) );
            if( Header->magic == MH_MAGIC_64 )
            {
                for( int32 i = 0; i < Header->ncmds; i++ )
                {
                    if( CurrentCommand->cmd == LC_LOAD_DYLIB )
                    {
                        struct dylib_command *DylibCommand = (struct dylib_command *) CurrentCommand;
                        Version = DylibCommand->dylib.current_version;
                        TimeStamp = DylibCommand->dylib.timestamp;
                        Version = ((Version & 0xff) + ((Version >> 8) & 0xff) * 100 + ((Version >> 16) & 0xffff) * 10000);
                        break;
                    }
                    
                    CurrentCommand = (struct load_command *)( (char *)CurrentCommand + CurrentCommand->cmdsize );
                }
            }
            
            // Module version
            WriteUTF16String(ReportFile, TEXT("\t\t<Parameter4>"));
            WriteUTF16String(ReportFile, ItoTCHAR(Version, 10));
            WriteLine(ReportFile, TEXT("</Parameter4>"));
            
            // Module time stamp
            WriteUTF16String(ReportFile, TEXT("\t\t<Parameter5>"));
            WriteUTF16String(ReportFile, ItoTCHAR(TimeStamp, 16));
            WriteLine(ReportFile, TEXT("</Parameter5>"));
            
            // MethodDef token -> no equivalent
            WriteLine(ReportFile, TEXT("\t\t<Parameter6>00000001</Parameter6>"));
            
            // IL Offset -> Function pointer
            WriteUTF16String(ReportFile, TEXT("\t\t<Parameter7>"));
            WriteUTF16String(ReportFile, ItoTCHAR((uint64)Info->si_addr, 16));
            WriteLine(ReportFile, TEXT("</Parameter7>"));
        }
        
        // Command line, must match the Windows version.
        WriteUTF16String(ReportFile, TEXT("\t\t<Parameter8>!"));
        WriteUTF16String(ReportFile, FCommandLine::GetOriginal());
        WriteLine(ReportFile, TEXT("!</Parameter8>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Parameter9>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.BranchBaseDir);
        WriteLine(ReportFile, TEXT("</Parameter9>"));
        
        WriteLine(ReportFile, TEXT("\t</ProblemSignatures>"));
        
        WriteLine(ReportFile, TEXT("\t<DynamicSignatures>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Parameter1>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.BiosUUID);
        WriteLine(ReportFile, TEXT("</Parameter1>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<Parameter2>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.LCID);
        WriteLine(ReportFile, TEXT("</Parameter2>"));
        WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<DeploymentName>%s</DeploymentName>"), FApp::GetDeploymentName()));
        WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<IsEnsure>%s</IsEnsure>"), bIsEnsure ? TEXT("1") : TEXT("0")));
        WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<IsAssert>%s</IsAssert>"), FDebug::bHasAsserted ? TEXT("1") : TEXT("0")));
        WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<CrashType>%s</CrashType>"), FGenericCrashContext::GetCrashTypeString(bIsEnsure, FDebug::bHasAsserted, GIsGPUCrashed)));
        WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<BuildVersion>%s</BuildVersion>"), FApp::GetBuildVersion()));
        WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<EngineModeEx>%s</EngineModeEx>"), FGenericCrashContext::EngineModeExString()));
        
        WriteLine(ReportFile, TEXT("\t</DynamicSignatures>"));
        
        WriteLine(ReportFile, TEXT("\t<SystemInformation>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<MID>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.MachineUUID);
        WriteLine(ReportFile, TEXT("</MID>"));
        
        WriteLine(ReportFile, TEXT("\t\t<SystemManufacturer>Apple Inc.</SystemManufacturer>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<SystemProductName>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.MachineModel);
        WriteLine(ReportFile, TEXT("</SystemProductName>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<BIOSVersion>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.BiosRelease);
        WriteUTF16String(ReportFile, TEXT("-"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.BiosRevision);
        WriteLine(ReportFile, TEXT("</BIOSVersion>"));
        
        WriteUTF16String(ReportFile, TEXT("\t\t<GraphicsCard>"));
        WriteUTF16String(ReportFile, *GIOSAppInfo.PrimaryGPU);
        WriteLine(ReportFile, TEXT("</GraphicsCard>"));
        
        WriteLine(ReportFile, TEXT("\t</SystemInformation>"));
        
        WriteLine(ReportFile, TEXT("</WERReportMetadata>"));
        
        close(ReportFile);
    }*/
}

void FIOSCrashContext::CopyMinidump(char const* OutputPath, char const* InputPath) const
{
#if !PLATFORM_TVOS
    NSError* Error = nil;
    NSString* Path = FString(ANSI_TO_TCHAR(InputPath)).GetNSString();
    NSData* CrashData = [NSData dataWithContentsOfFile: Path options: NSMappedRead error: &Error];
    if (CrashData && !Error)
    {
        PLCrashReport* CrashLog = [[PLCrashReport alloc] initWithData: CrashData error: &Error];
        if (CrashLog && !Error)
        {
            NSString* Report = [PLCrashReportTextFormatter stringValueForCrashReport: CrashLog withTextFormat: PLCrashReportTextFormatiOS];
            FString CrashDump = FString(Report);
            [Report writeToFile: Path atomically: YES encoding: NSUTF8StringEncoding error: &Error];
        }
        else
        {
            NSLog(@"****UE4 %@", [Error localizedDescription]);
        }
    }
    else
    {
        NSLog(@"****UE4 %@", [Error localizedDescription]);
    }
    int ReportFile = open(OutputPath, O_CREAT|O_WRONLY, 0766);
    int DumpFile = open(InputPath, O_RDONLY, 0766);
    if (ReportFile != -1 && DumpFile != -1)
    {
        char Data[PATH_MAX];
        
        int Bytes = 0;
        while((Bytes = read(DumpFile, Data, PATH_MAX)) > 0)
        {
            write(ReportFile, Data, Bytes);
        }
        
        close(DumpFile);
        close(ReportFile);
        
        unlink(InputPath);
    }
#endif
}

void FIOSCrashContext::GenerateInfoInFolder(char const* const InfoFolder, bool bIsEnsure) const
{
    // create a crash-specific directory
    char CrashInfoFolder[PATH_MAX] = {};
    FCStringAnsi::Strncpy(CrashInfoFolder, InfoFolder, PATH_MAX);
    
    if(!mkdir(CrashInfoFolder, 0766))
    {
        char FilePath[PATH_MAX] = {};
        FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
        FCStringAnsi::Strcat(FilePath, PATH_MAX, "/report.wer");
        int ReportFile = open(FilePath, O_CREAT|O_WRONLY, 0766);
        if (ReportFile != -1)
        {
            // write BOM
            static uint16 ByteOrderMarker = 0xFEFF;
            write(ReportFile, &ByteOrderMarker, sizeof(ByteOrderMarker));
            
            WriteUTF16String(ReportFile, TEXT("\r\nAppPath="));
            WriteUTF16String(ReportFile, *GIOSAppInfo.AppPath);
            WriteLine(ReportFile, TEXT("\r\n"));
            
            close(ReportFile);
        }
        
        // generate "WER"
        FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
        FCStringAnsi::Strcat(FilePath, PATH_MAX, "/wermeta.xml");
        GenerateWindowsErrorReport(FilePath, bIsEnsure);
        
        // generate "minidump" (Apple crash log format)
        FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
        FCStringAnsi::Strcat(FilePath, PATH_MAX, "/minidump.dmp");
        CopyMinidump(FilePath, GIOSAppInfo.PLCrashReportPath);
        
        // generate "info.txt" custom data for our server
        FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
        FCStringAnsi::Strcat(FilePath, PATH_MAX, "/info.txt");
        ReportFile = open(FilePath, O_CREAT|O_WRONLY, 0766);
        if (ReportFile != -1)
        {
            WriteUTF16String(ReportFile, TEXT("GameName UE4-"));
            WriteLine(ReportFile, *GIOSAppInfo.AppName);
            
            WriteUTF16String(ReportFile, TEXT("BuildVersion 1.0."));
            WriteUTF16String(ReportFile, ItoTCHAR(FEngineVersion::Current().GetChangelist() >> 16, 10));
            WriteUTF16String(ReportFile, TEXT("."));
            WriteLine(ReportFile, ItoTCHAR(FEngineVersion::Current().GetChangelist() & 0xffff, 10));
            
            WriteUTF16String(ReportFile, TEXT("CommandLine "));
            WriteLine(ReportFile, *GIOSAppInfo.CommandLine);
            
            WriteUTF16String(ReportFile, TEXT("BaseDir "));
            WriteLine(ReportFile, *GIOSAppInfo.BranchBaseDir);
            
            WriteUTF16String(ReportFile, TEXT("MachineGuid "));
            WriteLine(ReportFile, *GIOSAppInfo.MachineUUID);
            
            close(ReportFile);
        }
        
        // Introduces a new runtime crash context. Will replace all Windows related crash reporting.
        FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
        FCStringAnsi::Strcat(FilePath, PATH_MAX, "/" );
        FCStringAnsi::Strcat(FilePath, PATH_MAX, FGenericCrashContext::CrashContextRuntimeXMLNameA );
        //SerializeAsXML( FilePath ); @todo uncomment after verification - need to do a bit more work on this for macOS
        
        // copy log
        FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
        FCStringAnsi::Strcat(FilePath, PATH_MAX, "/");
        FCStringAnsi::Strcat(FilePath, PATH_MAX, (!GIOSAppInfo.AppName.IsEmpty() ? GIOSAppInfo.AppNameUTF8 : "UE4"));
        FCStringAnsi::Strcat(FilePath, PATH_MAX, ".log");
        
        int LogSrc = open(GIOSAppInfo.AppLogPath, O_RDONLY);
        int LogDst = open(FilePath, O_CREAT|O_WRONLY, 0766);
        
        char Data[PATH_MAX] = {};
        int Bytes = 0;
        while((Bytes = read(LogSrc, Data, PATH_MAX)) > 0)
        {
            write(LogDst, Data, Bytes);
        }
        
        // If present, include the crash report config file to pass config values to the CRC
        FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
        FCStringAnsi::Strcat(FilePath, PATH_MAX, "/");
        FCStringAnsi::Strcat(FilePath, PATH_MAX, FGenericCrashContext::CrashConfigFileNameA);
        int ConfigSrc = open(TCHAR_TO_ANSI(GetCrashConfigFilePath()), O_RDONLY);
        int ConfigDst = open(FilePath, O_CREAT | O_WRONLY, 0766);
        
        while ((Bytes = read(ConfigSrc, Data, PATH_MAX)) > 0)
        {
            write(ConfigDst, Data, Bytes);
        }
        
        close(ConfigDst);
        close(ConfigSrc);
        
        close(LogDst);
        close(LogSrc);
        // best effort, so don't care about result: couldn't copy -> tough, no log
    }
    else
    {
        NSLog(@"******* UE4 - Failed to make folder: %s", CrashInfoFolder);
    }
}

void FIOSCrashContext::GenerateCrashInfo() const
{
    // create a crash-specific directory
    char CrashInfoFolder[PATH_MAX] = {};
    FCStringAnsi::Strncpy(CrashInfoFolder, GIOSAppInfo.CrashReportPath, PATH_MAX);
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "/CrashReport-UE4-");
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, GIOSAppInfo.AppNameUTF8);
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-pid-");
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(getpid(), 10));
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-");
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.A, 16));
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.B, 16));
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.C, 16));
    FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.D, 16));
        
    const bool bIsEnsure = false;
    GenerateInfoInFolder(CrashInfoFolder, bIsEnsure);
        
    // for IOS we will need to send the report on the next run
    
    // Sandboxed applications re-raise the signal to trampoline into the system crash reporter as suppressing it may fall foul of Apple's Mac App Store rules.
    // @todo Submit an application to the MAS & see whether Apple's reviewers highlight our crash reporting or trampolining to the system reporter.
    if(GIOSAppInfo.bIsSandboxed)
    {
        struct sigaction Action;
        FMemory::Memzero(&Action, sizeof(struct sigaction));
        Action.sa_handler = SIG_DFL;
        sigemptyset(&Action.sa_mask);
        sigaction(SIGQUIT, &Action, NULL);
        sigaction(SIGILL, &Action, NULL);
        sigaction(SIGEMT, &Action, NULL);
        sigaction(SIGFPE, &Action, NULL);
        sigaction(SIGBUS, &Action, NULL);
        sigaction(SIGSEGV, &Action, NULL);
        sigaction(SIGSYS, &Action, NULL);
        sigaction(SIGABRT, &Action, NULL);
        sigaction(SIGTRAP, &Action, NULL);
        
        raise(Signal);
    }
    
    _Exit(0);
}

void FIOSCrashContext::GenerateEnsureInfo() const
{
    // Prevent CrashReportClient from spawning another CrashReportClient.
    const bool bCanRunCrashReportClient = FCString::Stristr( *(GIOSAppInfo.ExecutableName), TEXT( "CrashReportClient" ) ) == nullptr;
    
#if !PLATFORM_TVOS
    if(bCanRunCrashReportClient)
    {
        SCOPED_AUTORELEASE_POOL;
        
        // Write the PLCrashReporter report to the expected location
        NSData* CrashReport = [FIOSApplicationInfo::CrashReporter generateLiveReport];
        [CrashReport writeToFile:[NSString stringWithUTF8String:GIOSAppInfo.PLCrashReportPath] atomically:YES];
        
        // Use a slightly different output folder name to not conflict with a subequent crash
        const FGuid Guid = FGuid::NewGuid();
        FString GameName = FApp::GetProjectName();
        FString EnsureLogFolder = FString(GIOSAppInfo.CrashReportPath) / FString::Printf(TEXT("EnsureReport-%s-%s"), *GameName, *Guid.ToString(EGuidFormats::Digits));
        
        const bool bIsEnsure = true;
        GenerateInfoInFolder(TCHAR_TO_UTF8(*EnsureLogFolder), bIsEnsure);
        
        FString Arguments;
        if (IsInteractiveEnsureMode())
        {
            Arguments = FString::Printf(TEXT("\"%s/\""), *EnsureLogFolder);
        }
        else
        {
            Arguments = FString::Printf(TEXT("\"%s/\" -Unattended"), *EnsureLogFolder);
        }
        
        FString ReportClient = FPaths::ConvertRelativePathToFull(FPlatformProcess::GenerateApplicationPath(TEXT("CrashReportClient"), EBuildConfigurations::Development));
        FPlatformProcess::ExecProcess(*ReportClient, *Arguments, nullptr, nullptr, nullptr);
    }
#endif
}

static FCriticalSection EnsureLock;
static bool bReentranceGuard = false;

void NewReportEnsure( const TCHAR* ErrorMessage )
{
    // Simple re-entrance guard.
    EnsureLock.Lock();
    
    if( bReentranceGuard )
    {
        EnsureLock.Unlock();
        return;
    }
    
    bReentranceGuard = true;
    
#if !PLATFORM_TVOS
    if(FIOSApplicationInfo::CrashReporter != nil)
    {
        siginfo_t Signal;
        Signal.si_signo = SIGTRAP;
        Signal.si_code = TRAP_TRACE;
        Signal.si_addr = __builtin_return_address(0);
        
        FIOSCrashContext EnsureContext;
        EnsureContext.InitFromSignal(SIGTRAP, &Signal, nullptr);
        EnsureContext.GenerateEnsureInfo();
    }
#endif
    
    bReentranceGuard = false;
    EnsureLock.Unlock();
}
