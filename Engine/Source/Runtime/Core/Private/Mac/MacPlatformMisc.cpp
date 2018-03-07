// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformMisc.mm: Mac implementations of misc functions
=============================================================================*/

#include "MacPlatformMisc.h"
#include "Misc/App.h"
#include "ExceptionHandling.h"
#include "SecureHash.h"
#include "VarargsHelper.h"
#include "CocoaThread.h"
#include "EngineVersion.h"
#include "MacMallocZone.h"
#include "ApplePlatformSymbolication.h"
#include "MacPlatformCrashContext.h"
#include "PLCrashReporter.h"
#include "GenericPlatformDriver.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FeedbackContext.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Modules/ModuleManager.h"

#include <dlfcn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/kext/KextManager.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach-o/dyld.h>
#include <libproc.h>
#include <notify.h>
#include <uuid/uuid.h>

extern CORE_API bool GIsGPUCrashed;

/*------------------------------------------------------------------------------
 Console variables.
 ------------------------------------------------------------------------------*/

/** The selected explicit renderer ID. */
static int32 GMacExplicitRendererID = -1;
static FAutoConsoleVariableRef CVarMacExplicitRendererID(
	TEXT("Mac.ExplicitRendererID"),
	GMacExplicitRendererID,
	TEXT("Forces the Mac RHI to use the specified rendering device which is a 0-based index into the list of GPUs provided by FMacPlatformMisc::GetGPUDescriptors or -1 to disable & use the default device. (Default: -1, off)"),
	ECVF_RenderThreadSafe|ECVF_ReadOnly
	);

/*------------------------------------------------------------------------------
 FMacApplicationInfo - class to contain all state for crash reporting that is unsafe to acquire in a signal.
 ------------------------------------------------------------------------------*/

FMacMallocCrashHandler* GCrashMalloc = nullptr;

/**
 * Information that cannot be obtained during a signal-handler is initialised here.
 * This ensures that we only call safe functions within the crash reporting handler.
 */
struct FMacApplicationInfo
{
	void Init()
	{
		SCOPED_AUTORELEASE_POOL;
		
		// Prevent clang/ld from dead-code-eliminating the nothrow_t variants of global new.
		// This ensures that all OS calls to operator new go through our allocators and delete cleanly.
		{
			std::nothrow_t t;
			char* d = (char*)(operator new(8, t));
			delete d;
			
			d = (char*)operator new[]( 8, t );
			delete [] d;
		}
		
		AppName = FApp::GetProjectName();
		FCStringAnsi::Strcpy(AppNameUTF8, PATH_MAX+1, TCHAR_TO_UTF8(*AppName));
		
		ExecutableName = FPlatformProcess::ExecutableName();
		
		AppPath = FString([[NSBundle mainBundle] executablePath]);

		AppBundleID = FString([[NSBundle mainBundle] bundleIdentifier]);
		
		bIsUnattended = FApp::IsUnattended();

		bIsSandboxed = FPlatformProcess::IsSandboxedApplication();
		
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

		RunningOnMavericks = OSXVersion.majorVersion == 10 && OSXVersion.minorVersion == 9;

		XcodeVersion.majorVersion = XcodeVersion.minorVersion = XcodeVersion.patchVersion = 0;

		FPlatformProcess::ExecProcess(TEXT("/usr/bin/xcode-select"), TEXT("--print-path"), nullptr, &XcodePath, nullptr);
		if (XcodePath.Len() > 0)
		{
			XcodePath.RemoveAt(XcodePath.Len() - 1); // Remove \n at the end of the string
			if (IFileManager::Get().DirectoryExists(*XcodePath))
			{
				FString XcodeAppPath = XcodePath.Left(XcodePath.Find(TEXT(".app/")) + 4);
				NSBundle* XcodeBundle = [NSBundle bundleWithPath:XcodeAppPath.GetNSString()];
				if (XcodeBundle)
				{
					NSString* XcodeVersionString = (NSString*)[XcodeBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
					if (XcodeVersionString)
					{
						NSArray<NSString*>* VersionComponents = [XcodeVersionString componentsSeparatedByString:@"."];
						XcodeVersion.majorVersion = [[VersionComponents objectAtIndex:0] integerValue];
						XcodeVersion.minorVersion = VersionComponents.count > 1 ? [[VersionComponents objectAtIndex:1] integerValue] : 0;
						XcodeVersion.patchVersion = VersionComponents.count > 2 ? [[VersionComponents objectAtIndex:2] integerValue] : 0;
					}
				}
			}
			else
			{
				XcodePath.Empty();
			}
		}

		char TempSysCtlBuffer[PATH_MAX] = {};
		size_t TempSysCtlBufferSize = PATH_MAX;
		
		pid_t ParentPID = getppid();
		proc_pidpath(ParentPID, TempSysCtlBuffer, PATH_MAX);
		ParentProcess = TempSysCtlBuffer;
		
		MachineUUID = TEXT("00000000-0000-0000-0000-000000000000");
		io_service_t PlatformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,IOServiceMatching("IOPlatformExpertDevice"));
		if(PlatformExpert)
		{
			CFTypeRef SerialNumberAsCFString = IORegistryEntryCreateCFProperty(PlatformExpert,CFSTR(kIOPlatformUUIDKey),kCFAllocatorDefault, 0);
			if(SerialNumberAsCFString)
			{
				MachineUUID = FString((NSString*)SerialNumberAsCFString);
				CFRelease(SerialNumberAsCFString);
			}
			IOObjectRelease(PlatformExpert);
		}
		
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
		
		FString CrashVideoPath = FPaths::ProjectLogDir() + TEXT("CrashVideo.avi");

		// The engine mode may be incorrect at this point, as GIsEditor is uninitialized yet. We'll update BranchBaseDir in PostInitUpdate(),
		// but we initialize it here anyway in case the engine crashes before PostInitUpdate() is called.
		BranchBaseDir = FString::Printf( TEXT( "%s!%s!%s!%d" ), *FApp::GetBranchName(), FPlatformProcess::BaseDir(), FPlatformMisc::GetEngineMode(), FEngineVersion::Current().GetChangelist() );
		
		// Get the paths that the files will actually have been saved to
		FString LogDirectory = FPaths::ProjectLogDir();
		TCHAR CommandlineLogFile[MAX_SPRINTF]=TEXT("");
		
		// Use the log file specified on the commandline if there is one
		CommandLine = FCommandLine::Get();
		FString LogPath = FGenericPlatformOutputDevices::GetAbsoluteLogFilename();
		FCStringAnsi::Strcpy(AppLogPath, PATH_MAX + 1, TCHAR_TO_UTF8(*LogPath));

		FString UserCrashVideoPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CrashVideoPath);
		FCStringAnsi::Strcpy(CrashReportVideo, PATH_MAX+1, TCHAR_TO_UTF8(*UserCrashVideoPath));
		
		// Cache & create the crash report folder.
		FString ReportPath = FPaths::ConvertRelativePathToFull(FString::Printf(TEXT("%s"), *(FPaths::GameAgnosticSavedDir() / TEXT("Crashes"))));
		FCStringAnsi::Strcpy(CrashReportPath, PATH_MAX+1, TCHAR_TO_UTF8(*ReportPath));
		FString ReportClient = FPaths::ConvertRelativePathToFull(FPlatformProcess::GenerateApplicationPath(TEXT("CrashReportClient"), EBuildConfigurations::Development));
		FCStringAnsi::Strcpy(CrashReportClient, PATH_MAX+1, TCHAR_TO_UTF8(*ReportClient));
		IFileManager::Get().MakeDirectory(*ReportPath, true);
		
		// Notification handler to check we are running from a battery - this only applies to MacBook's.
		notify_handler_t PowerSourceNotifyHandler = ^(int32 Token){
			RunningOnBattery = false;
			CFTypeRef PowerSourcesInfo = IOPSCopyPowerSourcesInfo();
			if (PowerSourcesInfo)
			{
				CFArrayRef PowerSourcesArray = IOPSCopyPowerSourcesList(PowerSourcesInfo);
				for (CFIndex Index = 0; Index < CFArrayGetCount(PowerSourcesArray); Index++)
				{
					CFTypeRef PowerSource = CFArrayGetValueAtIndex(PowerSourcesArray, Index);
					NSDictionary* Description = (NSDictionary*)IOPSGetPowerSourceDescription(PowerSourcesInfo, PowerSource);
					if ([(NSString*)[Description objectForKey: @kIOPSPowerSourceStateKey] isEqualToString: @kIOPSBatteryPowerValue])
					{
						RunningOnBattery = true;
						break;
					}
				}
				CFRelease(PowerSourcesArray);
				CFRelease(PowerSourcesInfo);
			}
		};
		
		// Call now to fetch the status
		PowerSourceNotifyHandler(0);
		
		uint32 Status = notify_register_dispatch(kIOPSNotifyPowerSource, &PowerSourceNotification, dispatch_get_main_queue(), PowerSourceNotifyHandler);
		check(Status == NOTIFY_STATUS_OK);
		
		NumCores = FPlatformMisc::NumberOfCores();
		
		NSString* PLCrashReportFile = [TemporaryCrashReportFolder().GetNSString() stringByAppendingPathComponent:TemporaryCrashReportName().GetNSString()];
		[PLCrashReportFile getCString:PLCrashReportPath maxLength:PATH_MAX encoding:NSUTF8StringEncoding];
		
		SystemLogSize = 0;
		if (!bIsSandboxed)
		{
			SystemLogSize = IFileManager::Get().FileSize(TEXT("/var/log/system.log"));
		}
		
		if (!FPlatformMisc::IsDebuggerPresent() && FParse::Param(FCommandLine::Get(), TEXT("RedirectNSLog")))
		{
			fflush(stderr);
			StdErrPipe = [NSPipe new];
			int StdErr = dup2([StdErrPipe fileHandleForWriting].fileDescriptor, STDERR_FILENO);
			if(StdErr > 0)
			{
				@try
				{
					NSFileHandle* StdErrFile = [StdErrPipe fileHandleForReading];
					if(StdErrFile)
					{
						StdErrFile.readabilityHandler = ^(NSFileHandle* Handle){
							NSData* FileData = Handle.availableData;
							if (FileData.length > 0)
							{
								NSString* NewString = (NSString*)[[[NSString alloc] initWithData:FileData encoding:NSUTF8StringEncoding] autorelease];
								UE_LOG(LogMac, Error, TEXT("NSLog: %s"), *FString(NewString));
							}
						};
					}
				}
				@catch (NSException* Exc)
				{
					UE_LOG(LogMac, Warning, TEXT("Exception redirecting stderr to capture NSLog messages: %s"), *FString([Exc description]));
					[StdErrPipe release];
					StdErrPipe = nil;
				}
			}
			else
			{
				UE_LOG(LogMac, Warning, TEXT("Failed to redirect stderr in order to capture NSLog messages."));
				[StdErrPipe release];
				StdErrPipe = nil;
			}
		}
	}
	
	~FMacApplicationInfo()
	{
		if(GMalloc != GCrashMalloc)
		{
			delete GCrashMalloc;
		}
		if(CrashReporter)
		{
			CrashReporter = nil;
			[CrashReporter release];
		}
		if(PowerSourceNotification)
		{
			notify_cancel(PowerSourceNotification);
			PowerSourceNotification = 0;
		}
	}
	
	static FGuid RunGUID()
	{
		static FGuid Guid;
		if(!Guid.IsValid())
		{
			FPlatformMisc::CreateGuid(Guid);
		}
		return Guid;
	}
	
	static FString TemporaryCrashReportFolder()
	{
		static FString PLCrashReportFolder;
		if(PLCrashReportFolder.IsEmpty())
		{
			SCOPED_AUTORELEASE_POOL;
			
			NSArray* Paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
			NSString* CacheDir = [Paths objectAtIndex: 0];
			
			NSString* BundleID = [[NSBundle mainBundle] bundleIdentifier];
			if(!BundleID)
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
	
	bool bIsUnattended;
	bool bIsSandboxed;
	bool RunningOnBattery;
	bool RunningOnMavericks;
	int32 PowerSourceNotification;
	int32 NumCores;
	int64 SystemLogSize;
	char AppNameUTF8[PATH_MAX+1];
	char AppLogPath[PATH_MAX+1];
	char CrashReportPath[PATH_MAX+1];
	char PLCrashReportPath[PATH_MAX+1];
	char CrashReportClient[PATH_MAX+1];
	char CrashReportVideo[PATH_MAX+1];
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
	NSOperatingSystemVersion XcodeVersion;
	NSPipe* StdErrPipe;
	static PLCrashReporter* CrashReporter;
};
static FMacApplicationInfo GMacAppInfo;
PLCrashReporter* FMacApplicationInfo::CrashReporter = nullptr;

void FMacPlatformMisc::PlatformPreInit()
{
	FGenericPlatformMisc::PlatformPreInit();
	
	GMacAppInfo.Init();
	
	// No SIGPIPE crashes please - they are a pain to debug!
	signal(SIGPIPE, SIG_IGN);

	// Increase the maximum number of simultaneously open files
	uint32 MaxFilesPerProc = OPEN_MAX;
	size_t UInt32Size = sizeof(uint32);
	sysctlbyname("kern.maxfilesperproc", &MaxFilesPerProc, &UInt32Size, NULL, 0);

	struct rlimit Limit = {MaxFilesPerProc, RLIM_INFINITY};
	int32 Result = getrlimit(RLIMIT_NOFILE, &Limit);
	if (Result == 0)
	{
		if(Limit.rlim_max != RLIM_INFINITY)
		{
			UE_LOG(LogInit, Warning, TEXT("Hard Max File Limit Too Small: %llu, should be RLIM_INFINITY, UE4 may be unstable."), Limit.rlim_max);
		}
		if(Limit.rlim_max == RLIM_INFINITY)
		{
			Limit.rlim_cur = MaxFilesPerProc;
		}
		else
		{
			Limit.rlim_cur = FMath::Min(Limit.rlim_max, (rlim_t)MaxFilesPerProc);
		}
	}
	Result = setrlimit(RLIMIT_NOFILE, &Limit);
	if (Result != 0)
	{
		UE_LOG(LogInit, Warning, TEXT("Failed to change open file limit, UE4 may be unstable."));
	}
	
	FApplePlatformSymbolication::EnableCoreSymbolication(!FPlatformProcess::IsSandboxedApplication() && IS_PROGRAM);
}

void FMacPlatformMisc::PlatformInit()
{
	UE_LOG(LogInit, Log, TEXT("macOS %s (%s)"), *GMacAppInfo.OSVersion, *GMacAppInfo.OSBuild);
	UE_LOG(LogInit, Log, TEXT("Model: %s"), *GMacAppInfo.MachineModel);
	UE_LOG(LogInit, Log, TEXT("CPU: %s"), UTF8_TO_TCHAR(GMacAppInfo.MachineCPUString));
	
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("CPU Page size=%i, Cores=%i, HT=%i"), MemoryConstants.PageSize, FPlatformMisc::NumberOfCores(), FPlatformMisc::NumberOfCoresIncludingHyperthreads() );

	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName() );
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName() );

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle() );
	
	UE_LOG(LogInit, Log, TEXT("Power Source: %s"), GMacAppInfo.RunningOnBattery ? TEXT(kIOPSBatteryPowerValue) : TEXT(kIOPSACPowerValue) );

#if WITH_EDITOR
	if (GMacAppInfo.XcodePath.Len())
	{
		UE_LOG(LogInit, Log, TEXT("Xcode developer folder path: %s, version %d.%d.%d"), *GMacAppInfo.XcodePath, GMacAppInfo.XcodeVersion.majorVersion, GMacAppInfo.XcodeVersion.minorVersion, GMacAppInfo.XcodeVersion.patchVersion);
	}
	else
	{
		UE_LOG(LogInit, Log, TEXT("No Xcode installed"));
	}
#endif
}

void FMacPlatformMisc::PostInitMacAppInfoUpdate()
{
	GMacAppInfo.BranchBaseDir = FString::Printf(TEXT("%s!%s!%s!%d"), *FApp::GetBranchName(), FPlatformProcess::BaseDir(), FPlatformMisc::GetEngineMode(), FEngineVersion::Current().GetChangelist());
}

void FMacPlatformMisc::PlatformTearDown()
{
	FApplePlatformSymbolication::EnableCoreSymbolication(false);
	
	if (GMacAppInfo.StdErrPipe)
	{
		NSFileHandle* StdErrFile = [GMacAppInfo.StdErrPipe fileHandleForReading];
		if (StdErrFile)
		{
			StdErrFile.readabilityHandler = nil;
		}
		
		[GMacAppInfo.StdErrPipe release];
	}
}

void FMacPlatformMisc::SetEnvironmentVar(const TCHAR* InVariableName, const TCHAR* Value)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	if (Value == NULL || Value[0] == TEXT('\0'))
	{
		unsetenv(TCHAR_TO_ANSI(*VariableName));
	}
	else
	{
		setenv(TCHAR_TO_ANSI(*VariableName), TCHAR_TO_ANSI(Value), 1);
	}
}


TArray<uint8> FMacPlatformMisc::GetMacAddress()
{
	TArray<uint8> Result;

	io_iterator_t InterfaceIterator;
	{
		CFMutableDictionaryRef MatchingDict = IOServiceMatching(kIOEthernetInterfaceClass);

		if (!MatchingDict)
		{
			UE_LOG(LogMac, Warning, TEXT("GetMacAddress failed - no Ethernet interfaces"));
			return Result;
		}

		CFMutableDictionaryRef PropertyMatchDict =
			CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		if (!PropertyMatchDict)
		{
			UE_LOG(LogMac, Warning, TEXT("GetMacAddress failed - can't create CoreFoundation mutable dictionary!"));
			return Result;
		}

		CFDictionarySetValue(PropertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);
		CFDictionarySetValue(MatchingDict, CFSTR(kIOPropertyMatchKey), PropertyMatchDict);
		CFRelease(PropertyMatchDict);

		if (IOServiceGetMatchingServices(kIOMasterPortDefault, MatchingDict, &InterfaceIterator) != KERN_SUCCESS)
		{
			UE_LOG(LogMac, Warning, TEXT("GetMacAddress failed - error getting matching services"));
			return Result;
		}
	}

	io_object_t InterfaceService;
	while ( (InterfaceService = IOIteratorNext(InterfaceIterator)) != 0)
	{
		io_object_t ControllerService;
		if (IORegistryEntryGetParentEntry(InterfaceService, kIOServicePlane, &ControllerService) == KERN_SUCCESS)
		{
			CFTypeRef MACAddressAsCFData = IORegistryEntryCreateCFProperty(
				ControllerService, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
			if (MACAddressAsCFData)
			{
				Result.AddZeroed(kIOEthernetAddressSize);
				CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), Result.GetData());
				break;
				CFRelease(MACAddressAsCFData);
			}
			IOObjectRelease(ControllerService);
		}
		IOObjectRelease(InterfaceService);
	}
	IOObjectRelease(InterfaceIterator);

	return Result;
}

void FMacPlatformMisc::RequestExit( bool Force )
{
	UE_LOG(LogMac, Log,  TEXT("FPlatformMisc::RequestExit(%i)"), Force );
	
	notify_cancel(GMacAppInfo.PowerSourceNotification);
	GMacAppInfo.PowerSourceNotification = 0;
	
	if( Force )
	{
		// Exit immediately, by request.
		_Exit(GIsCriticalError ? 3 : 0);
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		GIsRequestingExit = 1;
	}
}

CORE_API TFunction<EAppReturnType::Type(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)> MessageBoxExtCallback;

EAppReturnType::Type FMacPlatformMisc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	if(MessageBoxExtCallback)
	{
		return MessageBoxExtCallback(MsgType, Text, Caption);
	}
	else
	{
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}
}

static bool HandleFirstInstall()
{
	if (FParse::Param( FCommandLine::Get(), TEXT("firstinstall")))
	{
		GLog->Flush();

		// Flush config to ensure language changes are written to disk.
		GConfig->Flush(false);

		return false; // terminate the game
	}
	return true; // allow the game to continue;
}

bool FMacPlatformMisc::CommandLineCommands()
{
	return HandleFirstInstall();
}

int32 FMacPlatformMisc::NumberOfCores()
{	
	static int32 NumberOfCores = -1;
	if (NumberOfCores == -1)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("usehyperthreading")))
		{
			NumberOfCores = NumberOfCoresIncludingHyperthreads();
		}
		else
		{
			SIZE_T Size = sizeof(int32);
		
			if (sysctlbyname("hw.physicalcpu", &NumberOfCores, &Size, NULL, 0) != 0)
			{
				NumberOfCores = 1;
			}
		}
	}
	return NumberOfCores;
}

int32 FMacPlatformMisc::NumberOfCoresIncludingHyperthreads()
{
	return FApplePlatformMisc::NumberOfCores();
}

void FMacPlatformMisc::NormalizePath(FString& InPath)
{
	SCOPED_AUTORELEASE_POOL;
	if (InPath.Len() > 1)
	{
#if WITH_EDITOR
		const bool bAppendSlash = InPath[InPath.Len() - 1] == '/'; // NSString will remove the trailing slash, if present, so we need to restore it after conversion
		InPath = [[InPath.GetNSString() stringByStandardizingPath] stringByResolvingSymlinksInPath];
		if (bAppendSlash)
		{
			InPath += TEXT("/");
		}
#else
		InPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		// This replacement addresses a "bug" where some callers
		// pass in paths that are badly composed with multiple
		// subdir separators.
		InPath.ReplaceInline(TEXT("//"), TEXT("/"));
		if (!InPath.IsEmpty() && InPath[InPath.Len() - 1] == TEXT('/'))
		{
			InPath.LeftChop(1);
		}
		// Remove redundant current-dir references.
		InPath.ReplaceInline(TEXT("/./"), TEXT("/"));
#endif
	}
}

FMacPlatformMisc::FGPUDescriptor::FGPUDescriptor()
: PCIDevice(0)
, GPUName(nil)
, GPUMetalBundle(nil)
, GPUOpenGLBundle(nil)
, GPUBundleID(nil)
, GPUVendorId(0)
, GPUDeviceId(0)
, GPUMemoryMB(0)
, GPUIndex(0)
, GPUHeadless(false)
{
}

FMacPlatformMisc::FGPUDescriptor::FGPUDescriptor(FGPUDescriptor const& Other)
: PCIDevice(0)
, GPUName(nil)
, GPUMetalBundle(nil)
, GPUOpenGLBundle(nil)
, GPUBundleID(nil)
, GPUVendorId(0)
, GPUDeviceId(0)
, GPUMemoryMB(0)
, GPUIndex(0)
, GPUHeadless(false)
{
	operator=(Other);
}

FMacPlatformMisc::FGPUDescriptor::~FGPUDescriptor()
{
	if(PCIDevice)
	{
		IOObjectRelease((io_registry_entry_t)PCIDevice);
	}
	[GPUName release];
	[GPUMetalBundle release];
	[GPUOpenGLBundle release];
	[GPUBundleID release];
}

FMacPlatformMisc::FGPUDescriptor& FMacPlatformMisc::FGPUDescriptor::operator=(FGPUDescriptor const& Other)
{
	if(this != &Other)
	{
		if(Other.PCIDevice)
		{
			IOObjectRetain((io_registry_entry_t)Other.PCIDevice);
		}
		if(PCIDevice)
		{
			IOObjectRelease((io_registry_entry_t)PCIDevice);
		}
		PCIDevice = Other.PCIDevice;
		
		
		[Other.GPUName retain];
		[Other.GPUMetalBundle retain];
		[Other.GPUOpenGLBundle retain];
		[Other.GPUBundleID retain];
		
		[GPUName release];
		[GPUMetalBundle release];
		[GPUOpenGLBundle release];
		[GPUBundleID release];

		GPUName = Other.GPUName;
		GPUMetalBundle = Other.GPUMetalBundle;
		GPUOpenGLBundle = Other.GPUOpenGLBundle;
		GPUBundleID = Other.GPUBundleID;
		
		GPUVendorId = Other.GPUVendorId;
		GPUDeviceId = Other.GPUDeviceId;
		GPUMemoryMB = Other.GPUMemoryMB;
		GPUIndex = Other.GPUIndex;
		GPUHeadless = Other.GPUHeadless;
	}
	return *this;
}

TMap<FString, float> FMacPlatformMisc::FGPUDescriptor::GetPerformanceStatistics() const
{
	SCOPED_AUTORELEASE_POOL;
	static CFStringRef PerformanceStatisticsRef = CFSTR("PerformanceStatistics");
	TMap<FString, float> Data;
	const CFDictionaryRef PerformanceStats = (const CFDictionaryRef)IORegistryEntrySearchCFProperty(PCIDevice, kIOServicePlane, PerformanceStatisticsRef, kCFAllocatorDefault, kIORegistryIterateRecursively);
	if(PerformanceStats)
	{
		if(CFGetTypeID(PerformanceStats) == CFDictionaryGetTypeID())
		{
			NSDictionary* PerformanceStatistics = (NSDictionary*)PerformanceStats;
			for(NSString* Key in PerformanceStatistics)
			{
				NSNumber* Value = [PerformanceStatistics objectForKey:Key];
				Data.Add(FString(Key), [Value floatValue]);
			}
		}
		CFRelease(PerformanceStats);
	}
	return Data;
}

TArray<FMacPlatformMisc::FGPUDescriptor> const& FMacPlatformMisc::GetGPUDescriptors()
{
	static TArray<FMacPlatformMisc::FGPUDescriptor> GPUs;
	if(GPUs.Num() == 0)
	{
		// Enumerate the GPUs via IOKit to avoid dragging in OpenGL
		io_iterator_t Iterator;
		CFMutableDictionaryRef MatchDictionary = IOServiceMatching("IOPCIDevice");
		if(IOServiceGetMatchingServices(kIOMasterPortDefault, MatchDictionary, &Iterator) == kIOReturnSuccess)
		{
			uint32 Index = 0;
			io_registry_entry_t ServiceEntry;
			while((ServiceEntry = IOIteratorNext(Iterator)))
			{
				CFMutableDictionaryRef ServiceInfo;
				if(IORegistryEntryCreateCFProperties(ServiceEntry, &ServiceInfo, kCFAllocatorDefault, kNilOptions) == kIOReturnSuccess)
				{
					// GPUs are class-code 0x30000
					static CFStringRef ClassCodeRef = CFSTR("class-code");
					const CFDataRef ClassCode = (const CFDataRef)CFDictionaryGetValue(ServiceInfo, ClassCodeRef);
					if(ClassCode && CFGetTypeID(ClassCode) == CFDataGetTypeID())
					{
						const uint32* ClassCodeValue = reinterpret_cast<const uint32*>(CFDataGetBytePtr(ClassCode));
						if(ClassCodeValue && *ClassCodeValue == 0x30000)
						{
							FMacPlatformMisc::FGPUDescriptor Desc;
							
							Desc.GPUIndex = Index++;
							
							IOObjectRetain(ServiceEntry);
							Desc.PCIDevice = (uint32)ServiceEntry;
							
							static CFStringRef ModelRef = CFSTR("model");
							const CFDataRef Model = (const CFDataRef)CFDictionaryGetValue(ServiceInfo, ModelRef);
							if(Model)
							{
								if(CFGetTypeID(Model) == CFDataGetTypeID())
								{
									CFStringRef ModelName = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, Model, kCFStringEncodingASCII);

									Desc.GPUName = (NSString*)ModelName;
								}
								else
								{
									CFRelease(Model);
								}
							}
							
							static CFStringRef DeviceIDRef = CFSTR("device-id");
							const CFDataRef DeviceID = (const CFDataRef)CFDictionaryGetValue(ServiceInfo, DeviceIDRef);
							if(DeviceID && CFGetTypeID(DeviceID) == CFDataGetTypeID())
							{
								const uint32* Value = reinterpret_cast<const uint32*>(CFDataGetBytePtr(DeviceID));
								Desc.GPUDeviceId = *Value;
							}
							
							static CFStringRef VendorIDRef = CFSTR("vendor-id");
							const CFDataRef VendorID = (const CFDataRef)CFDictionaryGetValue(ServiceInfo, VendorIDRef);
							if(DeviceID && CFGetTypeID(DeviceID) == CFDataGetTypeID())
							{
								const uint32* Value = reinterpret_cast<const uint32*>(CFDataGetBytePtr(VendorID));
								Desc.GPUVendorId = *Value;
							}
							
							static CFStringRef HeadlessRef = CFSTR("headless");
							const CFBooleanRef Headless = (const CFBooleanRef)CFDictionaryGetValue(ServiceInfo, HeadlessRef);
							if(Headless && CFGetTypeID(Headless) == CFBooleanGetTypeID())
							{
								Desc.GPUHeadless = (bool)CFBooleanGetValue(Headless);
							}
							
							static CFStringRef VRAMTotal = CFSTR("VRAM,totalMB");
							CFTypeRef VRAM = IORegistryEntrySearchCFProperty(ServiceEntry, kIOServicePlane, VRAMTotal, kCFAllocatorDefault, kIORegistryIterateRecursively);
							if (VRAM)
							{
								if(CFGetTypeID(VRAM) == CFDataGetTypeID())
								{
									const uint32* Value = reinterpret_cast<const uint32*>(CFDataGetBytePtr((CFDataRef)VRAM));
									Desc.GPUMemoryMB = *Value;
								}
								else if(CFGetTypeID(VRAM) == CFNumberGetTypeID())
								{
									CFNumberGetValue((CFNumberRef)VRAM, kCFNumberSInt32Type, &Desc.GPUMemoryMB);
								}
								CFRelease(VRAM);
							}
							
							static CFStringRef MetalPluginName = CFSTR("MetalPluginName");
							const CFStringRef MetalLibName = (const CFStringRef)IORegistryEntrySearchCFProperty(ServiceEntry, kIOServicePlane, MetalPluginName, kCFAllocatorDefault, kIORegistryIterateRecursively);
							if(MetalLibName)
							{
								if(CFGetTypeID(MetalLibName) == CFStringGetTypeID())
								{
									Desc.GPUMetalBundle = (NSString*)MetalLibName;
								}
								else
								{
									CFRelease(MetalLibName);
								}
							}
							
							static CFStringRef IOGLBundleName = CFSTR("IOGLBundleName");
							const CFStringRef OpenGLLibName = (const CFStringRef)IORegistryEntrySearchCFProperty(ServiceEntry, kIOServicePlane, IOGLBundleName, kCFAllocatorDefault, kIORegistryIterateRecursively);
							if(OpenGLLibName)
							{
								if(CFGetTypeID(OpenGLLibName) == CFStringGetTypeID())
								{
									Desc.GPUOpenGLBundle = (NSString*)OpenGLLibName;
								}
								else
								{
									CFRelease(OpenGLLibName);
								}
							}
							
							CFStringRef BundleID = nullptr;
							
							static CFStringRef CFBundleIdentifier = CFSTR("CFBundleIdentifier");
							
							io_iterator_t ChildIterator;
							if(IORegistryEntryGetChildIterator(ServiceEntry, kIOServicePlane, &ChildIterator) == kIOReturnSuccess)
							{
								io_registry_entry_t ChildEntry;
								while((BundleID == nullptr) && (ChildEntry = IOIteratorNext(ChildIterator)))
								{
									static CFStringRef IOMatchCategoryRef = CFSTR("IOMatchCategory");
									CFStringRef IOMatchCategory = (CFStringRef)IORegistryEntrySearchCFProperty(ChildEntry, kIOServicePlane, IOMatchCategoryRef, kCFAllocatorDefault, 0);
									static CFStringRef IOAcceleratorRef = CFSTR("IOAccelerator");
									if (IOMatchCategory && CFGetTypeID(IOMatchCategory) == CFStringGetTypeID() && CFStringCompare(IOMatchCategory, IOAcceleratorRef, 0) == kCFCompareEqualTo)
									{
										BundleID = (CFStringRef)IORegistryEntrySearchCFProperty(ChildEntry, kIOServicePlane, CFBundleIdentifier, kCFAllocatorDefault, 0);
									}
									if (IOMatchCategory)
									{
										CFRelease(IOMatchCategory);
									}
									IOObjectRelease(ChildEntry);
								}
								
								IOObjectRelease(ChildIterator);
							}
							
							if (BundleID == nullptr)
							{
								BundleID = (CFStringRef)IORegistryEntrySearchCFProperty(ServiceEntry, kIOServicePlane, CFBundleIdentifier, kCFAllocatorDefault, kIORegistryIterateRecursively);
							}
							
							if(BundleID)
							{
								if(CFGetTypeID(BundleID) == CFStringGetTypeID())
								{
									Desc.GPUBundleID = (NSString*)BundleID;
							}
								else
								{
									CFRelease(BundleID);
								}
							}
							
							GPUs.Add(Desc);
						}
					}
					CFRelease(ServiceInfo);
				}
				IOObjectRelease(ServiceEntry);
			}
			IOObjectRelease(Iterator);
		}
	}
	return GPUs;
}

int32 FMacPlatformMisc::GetExplicitRendererIndex()
{
	check(GConfig && GConfig->IsReadyForUse());
	
	int32 ExplicitRenderer = -1;
	if (FParse::Value(FCommandLine::Get(),TEXT("MacExplicitRenderer="), ExplicitRenderer) && ExplicitRenderer >= 0)
	{
		return ExplicitRenderer;
	}
	else
	{
		return GMacExplicitRendererID;
	}
}

FString FMacPlatformMisc::GetPrimaryGPUBrand()
{
	static FString PrimaryGPU;
	if(PrimaryGPU.IsEmpty())
	{
		TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = GetGPUDescriptors();
		
		if(GPUs.Num() > 1)
		{
			for(FMacPlatformMisc::FGPUDescriptor const& GPU : GPUs)
			{
				if(!GPU.GPUHeadless && GPU.GPUVendorId != 0x8086)
				{
					PrimaryGPU = GPU.GPUName;
					break;
				}
			}
		}
		
		if ( PrimaryGPU.IsEmpty() && GPUs.Num() > 0 )
		{
			PrimaryGPU = GPUs[0].GPUName;
		}
			
			if ( PrimaryGPU.IsEmpty() )
			{
				PrimaryGPU = FGenericPlatformMisc::GetPrimaryGPUBrand();
			}
		}
	return PrimaryGPU;
	}

FGPUDriverInfo FMacPlatformMisc::GetGPUDriverInfo(const FString& DeviceDescription)
{
	SCOPED_AUTORELEASE_POOL;

	FGPUDriverInfo Info;
	TArray<FString> NameComponents;
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = GetGPUDescriptors();
	
	for(FMacPlatformMisc::FGPUDescriptor const& GPU : GPUs)
	{
		NameComponents.Empty();
		bool bMatchesName = FString(GPU.GPUName).TrimStart().ParseIntoArray(NameComponents, TEXT(" ")) > 0;
		for (FString& Component : NameComponents)
		{
			bMatchesName &= DeviceDescription.Contains(Component);
		}
		
		if (bMatchesName)
		{
			Info.VendorId = GPU.GPUVendorId;
			Info.DeviceDescription = FString(GPU.GPUName);

			if (Info.IsAMD())
			{
				Info.ProviderName = TEXT("AMD");
			}
			else if (Info.IsIntel())
			{
				Info.ProviderName = TEXT("Intel");
			}
			else if (Info.IsNVIDIA())
			{
				Info.ProviderName = TEXT("Nvidia");
			}
			else
			{
				Info.ProviderName = TEXT("Apple");
			}
			
			bool bGotInternalVersionInfo = false;
			bool bGotUserVersionInfo = false;
			bool bGotDate = false;
			
			for(uint32 Index = 0; Index < _dyld_image_count(); Index++)
			{
				char const* IndexName = _dyld_get_image_name(Index);
				FString FullModulePath(IndexName);
				FString Name = FPaths::GetBaseFilename(FullModulePath);
				if(Name == FString(GPU.GPUMetalBundle) || Name == FString(GPU.GPUOpenGLBundle))
				{
					struct mach_header_64 const* IndexModule64 = NULL;
					struct load_command const* LoadCommands = NULL;
					
					struct mach_header const* IndexModule32 = _dyld_get_image_header(Index);
					check(IndexModule32->magic == MH_MAGIC_64);
					
					IndexModule64 = (struct mach_header_64 const*)IndexModule32;
					LoadCommands = (struct load_command const*)(IndexModule64 + 1);
					struct load_command const* Command = LoadCommands;
					struct dylib_command const* DylibID = nullptr;
					struct source_version_command const* SourceVersion = nullptr;
					for(uint32 CommandIndex = 0; CommandIndex < IndexModule64->ncmds; CommandIndex++)
					{
						if (Command && Command->cmd == LC_ID_DYLIB)
						{
							DylibID = (struct dylib_command const*)Command;
							break;
						}
						else if(Command && Command->cmd == LC_SOURCE_VERSION)
						{
							SourceVersion = (struct source_version_command const*)Command;
						}
						Command = (struct load_command const*)(((char const*)Command) + Command->cmdsize);
					}
					if(DylibID)
					{
						uint32 Major = ((DylibID->dylib.current_version >> 16) & 0xffff);
						uint32 Minor = ((DylibID->dylib.current_version >> 8) & 0xff);
						uint32 Patch = ((DylibID->dylib.current_version & 0xff));
						Info.InternalDriverVersion = FString::Printf(TEXT("%d.%d.%d"), Major, Minor, Patch);
						
						time_t DylibTime = (time_t)DylibID->dylib.timestamp;
						struct tm Time;
						gmtime_r(&DylibTime, &Time);
						Info.DriverDate = FString::Printf(TEXT("%d-%d-%d"), Time.tm_mon + 1, Time.tm_mday, 1900 + Time.tm_year);

						bGotInternalVersionInfo = Major != 0 || Minor != 0 || Patch != 0;
						bGotDate = (1900 + Time.tm_year) >= 2014;
						break;
					}
					else if (SourceVersion)
					{
						uint32 A = ((SourceVersion->version >> 40) & 0xffffff);
						uint32 B = ((SourceVersion->version >> 30) & 0x3ff);
						uint32 C = ((SourceVersion->version >> 20) & 0x3ff);
						uint32 D = ((SourceVersion->version >> 10) & 0x3ff);
						uint32 E = (SourceVersion->version & 0x3ff);
						Info.InternalDriverVersion = FString::Printf(TEXT("%d.%d.%d.%d.%d"), A, B, C, D, E);
						
						struct stat Stat;
						stat(IndexName, &Stat);
						
						struct tm Time;
						gmtime_r(&Stat.st_mtime, &Time);
						Info.DriverDate = FString::Printf(TEXT("%d-%d-%d"), Time.tm_mon + 1, Time.tm_mday, 1900 + Time.tm_year);
						
						bGotInternalVersionInfo = A != 0 || B != 0 || C != 0 || D != 0;
						bGotDate = (1900 + Time.tm_year) >= 2014;
					}
				}
			}
			
			if(!GMacAppInfo.bIsSandboxed)
			{
				if(!bGotDate || !bGotInternalVersionInfo || !bGotUserVersionInfo)
				{
					NSURL* URL = (NSURL*)KextManagerCreateURLForBundleIdentifier(kCFAllocatorDefault, (CFStringRef)GPU.GPUBundleID);
					if(URL)
					{
						NSBundle* ControllerBundle = [NSBundle bundleWithURL:URL];
						if(ControllerBundle)
						{
							NSDictionary* Dict = ControllerBundle.infoDictionary;
							NSString* BundleVersion = [Dict objectForKey:@"CFBundleVersion"];
							NSString* BundleShortVersion = [Dict objectForKey:@"CFBundleShortVersionString"];
							NSString* BundleInfoVersion = [Dict objectForKey:@"CFBundleGetInfoString"];
							if (!bGotInternalVersionInfo && (BundleVersion || BundleShortVersion))
							{
								Info.InternalDriverVersion = FString(BundleShortVersion ? BundleShortVersion : BundleVersion);
								bGotInternalVersionInfo = true;
							}
							if (!bGotUserVersionInfo && BundleInfoVersion)
							{
								Info.UserDriverVersion = FString(BundleInfoVersion);
								bGotUserVersionInfo = true;
							}
							
							if(!bGotDate)
							{
								NSURL* Exe = ControllerBundle.executableURL;
								if (Exe)
								{
									id Value = nil;
									if([Exe getResourceValue:&Value forKey:NSURLContentModificationDateKey error:nil] && Value)
									{
										NSDate* Date = (NSDate*)Value;
										Info.DriverDate = [Date descriptionWithLocale:nil];
										bGotDate = true;
									}
								}
							}
						}
					}
				}
				
				if(!bGotInternalVersionInfo)
				{
					NSArray* Array = [NSArray arrayWithObject: GPU.GPUBundleID];
					NSDictionary* Dict = (NSDictionary*)KextManagerCopyLoadedKextInfo((CFArrayRef)Array, nil);
					if(Dict)
					{
						NSDictionary* ControllerDict = [Dict objectForKey:GPU.GPUBundleID];
						if(ControllerDict)
						{
							NSString* BundleVersion = [ControllerDict objectForKey:@"CFBundleVersion"];
							Info.InternalDriverVersion = FString(BundleVersion);
						}
						[Dict release];
					}
				}
			}
			else if(bGotInternalVersionInfo && !bGotUserVersionInfo)
			{
				Info.UserDriverVersion = Info.InternalDriverVersion;
			}
			
			break;
		}
	}
	
	return Info;
}

void FMacPlatformMisc::GetOSVersions( FString& out_OSVersionLabel, FString& out_OSSubVersionLabel )
{
	out_OSVersionLabel = GMacAppInfo.OSVersion;
	out_OSSubVersionLabel = GMacAppInfo.OSBuild;
}

FString FMacPlatformMisc::GetOSVersion()
{
	return GMacAppInfo.OSVersion;
}

bool FMacPlatformMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
	struct statfs FSStat = { 0 };
	FTCHARToUTF8 Converter(*InPath);
	int Err = statfs((ANSICHAR*)Converter.Get(), &FSStat);
	if (Err == 0)
	{
		TotalNumberOfBytes = FSStat.f_blocks * FSStat.f_bsize;
		NumberOfFreeBytes = FSStat.f_bavail * FSStat.f_bsize;
	}
	return (Err == 0);
}

bool FMacPlatformMisc::HasSeparateChannelForDebugOutput()
{
	return FPlatformMisc::IsDebuggerPresent() || isatty(STDOUT_FILENO) || isatty(STDERR_FILENO);
}

FString FMacPlatformMisc::GetCPUVendor()
{
	union
	{
		char Buffer[12+1];
		struct
		{
			int dw0;
			int dw1;
			int dw2;
		} Dw;
	} VendorResult;


	int32 Args[4];
	asm( "cpuid" : "=a" (Args[0]), "=b" (Args[1]), "=c" (Args[2]), "=d" (Args[3]) : "a" (0));

	VendorResult.Dw.dw0 = Args[1];
	VendorResult.Dw.dw1 = Args[3];
	VendorResult.Dw.dw2 = Args[2];
	VendorResult.Buffer[12] = 0;

	return ANSI_TO_TCHAR(VendorResult.Buffer);
}

uint32 FMacPlatformMisc::GetCPUInfo()
{
	uint32 Args[4];
	asm( "cpuid" : "=a" (Args[0]), "=b" (Args[1]), "=c" (Args[2]), "=d" (Args[3]) : "a" (1));

	return Args[0];
}

FText FMacPlatformMisc::GetFileManagerName()
{
	return NSLOCTEXT("MacPlatform", "FileManagerName", "Finder");
}

bool FMacPlatformMisc::IsRunningOnBattery()
{
	return GMacAppInfo.RunningOnBattery;
}

bool FMacPlatformMisc::IsRunningOnMavericks()
{
	return GMacAppInfo.RunningOnMavericks;
}

int32 FMacPlatformMisc::MacOSXVersionCompare(uint8 Major, uint8 Minor, uint8 Revision)
{
	uint8 TargetValues[3] = {Major, Minor, Revision};
	NSInteger ComponentValues[3] = {GMacAppInfo.OSXVersion.majorVersion, GMacAppInfo.OSXVersion.minorVersion, GMacAppInfo.OSXVersion.patchVersion};
	
	for(uint32 i = 0; i < 3; i++)
	{
		if(ComponentValues[i] < TargetValues[i])
		{
			return -1;
		}
		else if(ComponentValues[i] > TargetValues[i])
		{
			return 1;
		}
	}
	
	return 0;
}

FString FMacPlatformMisc::GetOperatingSystemId()
{
	FString Result;
	io_service_t Entry = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
	if (Entry)
	{
		CFTypeRef UUID = IORegistryEntryCreateCFProperty(Entry, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
		Result = FString((__bridge NSString*)UUID);
		IOObjectRelease(Entry);
		CFRelease(UUID);
	}
	else
	{
		UE_LOG(LogMac, Warning, TEXT("GetOperatingSystemId() failed"));
	}
	return Result;
}

FString FMacPlatformMisc::GetXcodePath()
{
	return GMacAppInfo.XcodePath;
}

bool FMacPlatformMisc::IsSupportedXcodeVersionInstalled()
{
	// We need Xcode 8.2 or newer to be able to compile Metal shaders correctly
	return GMacAppInfo.XcodeVersion.majorVersion > 8 || (GMacAppInfo.XcodeVersion.majorVersion == 8 && GMacAppInfo.XcodeVersion.minorVersion >= 2);
}

/** Global pointer to crash handler */
void (* GCrashHandlerPointer)(const FGenericCrashContext& Context) = NULL;

/**
 * Good enough default crash reporter.
 */
static void DefaultCrashHandler(FMacCrashContext const& Context)
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
	
	return Context.GenerateCrashInfoAndLaunchReporter();
}

/** Number of stack entries to ignore in backtrace */
static uint32 GMacStackIgnoreDepth = 6;

/** True system-specific crash handler that gets called first */
static void PlatformCrashHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	// Disable CoreSymbolication
	FApplePlatformSymbolication::EnableCoreSymbolication( false );
	
	FMacCrashContext CrashContext;
	CrashContext.IgnoreDepth = GMacStackIgnoreDepth;
	CrashContext.InitFromSignal(Signal, Info, Context);
	
	// Switch to crash handler malloc to avoid malloc reentrancy
	check(GCrashMalloc);
	GCrashMalloc->Enable(&CrashContext, FPlatformTLS::GetCurrentThreadId());
	
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

/**
 * Handles graceful termination. Gives time to exit gracefully, but second signal will quit immediately.
 */
static void GracefulTerminationHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	// make sure as much data is written to disk as possible
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
	
	if( !GIsRequestingExit )
	{
		GIsRequestingExit = 1;
	}
	else
	{
		_Exit(0);
	}
}

void FMacPlatformMisc::SetGracefulTerminationHandler()
{
	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	Action.sa_sigaction = GracefulTerminationHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sigaction(SIGINT, &Action, NULL);
	sigaction(SIGTERM, &Action, NULL);
	sigaction(SIGHUP, &Action, NULL);	//  this should actually cause the server to just re-read configs (restart?)
}

void FMacPlatformMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context))
{
	SCOPED_AUTORELEASE_POOL;
	
	GCrashHandlerPointer = CrashHandler;
	
	if(!FMacApplicationInfo::CrashReporter && !GCrashMalloc)
	{
		// Configure the crash handler malloc zone to reserve some VM space for itself
		GCrashMalloc = new FMacMallocCrashHandler( 128 * 1024 * 1024 );
		
		PLCrashReporterConfig* Config = [[[PLCrashReporterConfig alloc] initWithSignalHandlerType: PLCrashReporterSignalHandlerTypeBSD
																		symbolicationStrategy: PLCrashReporterSymbolicationStrategyNone
																		crashReportFolder:FMacApplicationInfo::TemporaryCrashReportFolder().GetNSString()
																		crashReportName:FMacApplicationInfo::TemporaryCrashReportName().GetNSString()] autorelease];
		FMacApplicationInfo::CrashReporter = [[PLCrashReporter alloc] initWithConfiguration: Config];
		
		PLCrashReporterCallbacks CrashReportCallback = {
			.version = 0,
			.context = nullptr,
			.handleSignal = PLCrashReporterHandler
		};
		
		[FMacApplicationInfo::CrashReporter setCrashCallbacks: &CrashReportCallback];

		NSError* Error = nil;
		if ([FMacApplicationInfo::CrashReporter enableCrashReporterAndReturnError: &Error])
		{
			GMacStackIgnoreDepth = 0;
		}
		else
		{
			UE_LOG(LogMac, Log,  TEXT("Failed to enable PLCrashReporter: %s"), *FString([Error localizedDescription]) );
			
			UE_LOG(LogMac, Log,  TEXT("Falling back to native signal handlers."));
			
			struct sigaction Action;
			FMemory::Memzero(&Action, sizeof(struct sigaction));
			Action.sa_sigaction = PlatformCrashHandler;
			sigemptyset(&Action.sa_mask);
			Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
			sigaction(SIGQUIT, &Action, NULL);	// SIGQUIT is a user-initiated "crash".
			sigaction(SIGILL, &Action, NULL);
			sigaction(SIGEMT, &Action, NULL);
			sigaction(SIGFPE, &Action, NULL);
			sigaction(SIGBUS, &Action, NULL);
			sigaction(SIGSEGV, &Action, NULL);
			sigaction(SIGSYS, &Action, NULL);
			sigaction(SIGABRT, &Action, NULL);
		}
	}
}

void FMacCrashContext::GenerateWindowsErrorReport(char const* WERPath, bool bIsEnsure) const
{
	int ReportFile = open(WERPath, O_CREAT|O_WRONLY, 0766);
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
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteLine(ReportFile, TEXT("</WindowsNTVersion>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Build>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteUTF16String(ReportFile, TEXT(" ("));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSBuild);
		WriteLine(ReportFile, TEXT(")</Build>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Product>(0x30): Mac OS X "));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteLine(ReportFile, TEXT("</Product>"));
		
		WriteLine(ReportFile, TEXT("\t\t<Edition>Mac OS X</Edition>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<BuildString>Mac OS X "));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteUTF16String(ReportFile, TEXT(" ("));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSBuild);
		WriteLine(ReportFile, TEXT(")</BuildString>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Revision>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSBuild);
		WriteLine(ReportFile, TEXT("</Revision>"));
		
		WriteLine(ReportFile, TEXT("\t\t<Flavor>Multiprocessor Free</Flavor>"));
		WriteLine(ReportFile, TEXT("\t\t<Architecture>X64</Architecture>"));
		WriteUTF16String(ReportFile, TEXT("\t\t<LCID>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.LCID);
		WriteLine(ReportFile, TEXT("</LCID>"));
		WriteLine(ReportFile, TEXT("\t</OSVersionInformation>"));
		
		WriteLine(ReportFile, TEXT("\t<ParentProcessInformation>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<ParentProcessId>"));
		WriteUTF16String(ReportFile, ItoTCHAR(getppid(), 10));
		WriteLine(ReportFile, TEXT("</ParentProcessId>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<ParentProcessPath>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.ParentProcess);
		WriteLine(ReportFile, TEXT("</ParentProcessPath>"));
		
		WriteLine(ReportFile, TEXT("\t\t<ParentProcessCmdLine></ParentProcessCmdLine>"));	// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t</ParentProcessInformation>"));
		
		WriteLine(ReportFile, TEXT("\t<ProblemSignatures>"));
		WriteLine(ReportFile, TEXT("\t\t<EventType>APPCRASH</EventType>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter0>UE4-"));
		WriteUTF16String(ReportFile, *GMacAppInfo.AppName);
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
		WriteUTF16String(ReportFile, *GMacAppInfo.BranchBaseDir);
		WriteLine(ReportFile, TEXT("</Parameter9>"));
		
		WriteLine(ReportFile, TEXT("\t</ProblemSignatures>"));
		
		WriteLine(ReportFile, TEXT("\t<DynamicSignatures>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter1>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.BiosUUID);
		WriteLine(ReportFile, TEXT("</Parameter1>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter2>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.LCID);
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
		WriteUTF16String(ReportFile, *GMacAppInfo.MachineUUID);
		WriteLine(ReportFile, TEXT("</MID>"));
		
		WriteLine(ReportFile, TEXT("\t\t<SystemManufacturer>Apple Inc.</SystemManufacturer>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<SystemProductName>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.MachineModel);
		WriteLine(ReportFile, TEXT("</SystemProductName>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<BIOSVersion>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.BiosRelease);
		WriteUTF16String(ReportFile, TEXT("-"));
		WriteUTF16String(ReportFile, *GMacAppInfo.BiosRevision);
		WriteLine(ReportFile, TEXT("</BIOSVersion>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<GraphicsCard>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.PrimaryGPU);
		WriteLine(ReportFile, TEXT("</GraphicsCard>"));
		
		WriteLine(ReportFile, TEXT("\t</SystemInformation>"));
		
		WriteLine(ReportFile, TEXT("</WERReportMetadata>"));
		
		close(ReportFile);
	}
}

void FMacCrashContext::CopyMinidump(char const* OutputPath, char const* InputPath) const
{
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
}

void FMacCrashContext::GenerateInfoInFolder(char const* const InfoFolder, bool bIsEnsure) const
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
			WriteUTF16String(ReportFile, *GMacAppInfo.AppPath);
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
		CopyMinidump(FilePath, GMacAppInfo.PLCrashReportPath);
		
		// generate "info.txt" custom data for our server
		FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		FCStringAnsi::Strcat(FilePath, PATH_MAX, "/info.txt");
		ReportFile = open(FilePath, O_CREAT|O_WRONLY, 0766);
		if (ReportFile != -1)
		{
			WriteUTF16String(ReportFile, TEXT("GameName UE4-"));
			WriteLine(ReportFile, *GMacAppInfo.AppName);
			
			WriteUTF16String(ReportFile, TEXT("BuildVersion 1.0."));
			WriteUTF16String(ReportFile, ItoTCHAR(FEngineVersion::Current().GetChangelist() >> 16, 10));
			WriteUTF16String(ReportFile, TEXT("."));
			WriteLine(ReportFile, ItoTCHAR(FEngineVersion::Current().GetChangelist() & 0xffff, 10));
			
			WriteUTF16String(ReportFile, TEXT("CommandLine "));
			WriteLine(ReportFile, *GMacAppInfo.CommandLine);
			
			WriteUTF16String(ReportFile, TEXT("BaseDir "));
			WriteLine(ReportFile, *GMacAppInfo.BranchBaseDir);
			
			WriteUTF16String(ReportFile, TEXT("MachineGuid "));
			WriteLine(ReportFile, *GMacAppInfo.MachineUUID);
			
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
		FCStringAnsi::Strcat(FilePath, PATH_MAX, (!GMacAppInfo.AppName.IsEmpty() ? GMacAppInfo.AppNameUTF8 : "UE4"));
		FCStringAnsi::Strcat(FilePath, PATH_MAX, ".log");
		int LogSrc = open(GMacAppInfo.AppLogPath, O_RDONLY);
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

		// Copy the system log to capture GPU restarts and other nasties not reported by our application
		if ( !GMacAppInfo.bIsSandboxed && GMacAppInfo.SystemLogSize >= 0 && access("/var/log/system.log", R_OK|F_OK) == 0 )
		{
			char const* SysLogHeader = "\nAppending System Log:\n";
			write(LogDst, SysLogHeader, strlen(SysLogHeader));
			
			int SysLogSrc = open("/var/log/system.log", O_RDONLY);
			
			// Attempt to capture only the system log from while our application was running but
			if (lseek(SysLogSrc, GMacAppInfo.SystemLogSize, SEEK_SET) != GMacAppInfo.SystemLogSize)
			{
				close(SysLogSrc);
				SysLogSrc = open("/var/log/system.log", O_RDONLY);
			}
			
			while((Bytes = read(SysLogSrc, Data, PATH_MAX)) > 0)
			{
				write(LogDst, Data, Bytes);
			}
			close(SysLogSrc);
		}
		
		close(LogDst);
		close(LogSrc);
		// best effort, so don't care about result: couldn't copy -> tough, no log
		
		// copy crash video if there is one
		if ( access(GMacAppInfo.CrashReportVideo, R_OK|F_OK) == 0 )
		{
			FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
			FCStringAnsi::Strcat(FilePath, PATH_MAX, "/");
			FCStringAnsi::Strcat(FilePath, PATH_MAX, "CrashVideo.avi");
			int VideoSrc = open(GMacAppInfo.CrashReportVideo, O_RDONLY);
			int VideoDst = open(FilePath, O_CREAT|O_WRONLY, 0766);
			
			while((Bytes = read(VideoSrc, Data, PATH_MAX)) > 0)
			{
				write(VideoDst, Data, Bytes);
			}
			close(VideoDst);
			close(VideoSrc);
		}
	}
}

void FMacCrashContext::GenerateCrashInfoAndLaunchReporter() const
{
	// Prevent CrashReportClient from spawning another CrashReportClient.
	const bool bCanRunCrashReportClient = FCString::Stristr( *(GMacAppInfo.ExecutableName), TEXT( "CrashReportClient" ) ) == nullptr;

	if(bCanRunCrashReportClient)
	{
		// create a crash-specific directory
		char CrashInfoFolder[PATH_MAX] = {};
		FCStringAnsi::Strncpy(CrashInfoFolder, GMacAppInfo.CrashReportPath, PATH_MAX);
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "/CrashReport-UE4-");
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, GMacAppInfo.AppNameUTF8);
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-pid-");
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(getpid(), 10));
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-");
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.A, 16));
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.B, 16));
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.C, 16));
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.D, 16));
		
		const bool bIsEnsure = false;
		GenerateInfoInFolder(CrashInfoFolder, bIsEnsure);

		// try launching the tool and wait for its exit, if at all
		// Use vfork() & execl() as they are async-signal safe, CreateProc can fail in Cocoa
		int32 ReturnCode = 0;
		FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "/");
		pid_t ForkPID = vfork();
		if(ForkPID == 0)
		{
			// Child
			if(GMacAppInfo.bIsUnattended)
			{
				execl(GMacAppInfo.CrashReportClient, "CrashReportClient", CrashInfoFolder, "-Unattended", NULL);
			}
			else
			{
				execl(GMacAppInfo.CrashReportClient, "CrashReportClient", CrashInfoFolder, NULL);
			}
		}
		// We no longer wait here because on return the OS will scribble & crash again due to the behaviour of the XPC function
		// macOS uses internally to launch/wait on the CrashReportClient. It is simpler, easier & safer to just die here like a good little Mac.app.
	}
	
	// Sandboxed applications re-raise the signal to trampoline into the system crash reporter as suppressing it may fall foul of Apple's Mac App Store rules.
	// @todo Submit an application to the MAS & see whether Apple's reviewers highlight our crash reporting or trampolining to the system reporter.
	if(GMacAppInfo.bIsSandboxed)
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

void FMacCrashContext::GenerateEnsureInfoAndLaunchReporter() const
{
	// Prevent CrashReportClient from spawning another CrashReportClient.
	const bool bCanRunCrashReportClient = FCString::Stristr( *(GMacAppInfo.ExecutableName), TEXT( "CrashReportClient" ) ) == nullptr;
	
	if(bCanRunCrashReportClient)
	{
		SCOPED_AUTORELEASE_POOL;
		
		// Write the PLCrashReporter report to the expected location
		NSData* CrashReport = [FMacApplicationInfo::CrashReporter generateLiveReport];
		[CrashReport writeToFile:[NSString stringWithUTF8String:GMacAppInfo.PLCrashReportPath] atomically:YES];

		// Use a slightly different output folder name to not conflict with a subequent crash
		const FGuid Guid = FGuid::NewGuid();
		FString GameName = FApp::GetProjectName();
		FString EnsureLogFolder = FString(GMacAppInfo.CrashReportPath) / FString::Printf(TEXT("EnsureReport-%s-%s"), *GameName, *Guid.ToString(EGuidFormats::Digits));
		
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
	
	if(FMacApplicationInfo::CrashReporter != nil)
	{
		siginfo_t Signal;
		Signal.si_signo = SIGTRAP;
		Signal.si_code = TRAP_TRACE;
		Signal.si_addr = __builtin_return_address(0);
		
		FMacCrashContext EnsureContext;
		EnsureContext.InitFromSignal(SIGTRAP, &Signal, nullptr);
		EnsureContext.GenerateEnsureInfoAndLaunchReporter();
	}
	
	bReentranceGuard = false;
	EnsureLock.Unlock();
}
typedef NSArray* (*MTLCopyAllDevices)(void);

bool FMacPlatformMisc::HasPlatformFeature(const TCHAR* FeatureName)
{
	if (FCString::Stricmp(FeatureName, TEXT("Metal")) == 0)
	{
		bool bHasMetal = false;
		
		if (FModuleManager::Get().ModuleExists(TEXT("MetalRHI")))
		{
			// Find out if there are any Metal devices on the system - some Mac's have none
			void* DLLHandle = FPlatformProcess::GetDllHandle(TEXT("/System/Library/Frameworks/Metal.framework/Metal"));
			if (DLLHandle)
			{
				TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
				for (FMacPlatformMisc::FGPUDescriptor const& GPU : GPUs)
				{
					if (GPU.GPUMetalBundle && GPU.GPUMetalBundle.length > 0)
					{
						bHasMetal = true;
						break;
					}
				}
				
				FPlatformProcess::FreeDllHandle(DLLHandle);
			}
		}

		return bHasMetal;
	}

	return FGenericPlatformMisc::HasPlatformFeature(FeatureName);
}

/*------------------------------------------------------------------------------
 DriverMonitor - Stats groups for Mac Driver Monitor performance statistics available from IOKit & Driver Monitor, so that they may be logged within our performance capture tools.
    These stats provide a lot of information about what the driver is doing at any point and being able to see where the time & memory is going can be very helpful when debugging.
    They would be especially helpful if they could be logged over time alongside out own RHI stats to compare what we *think* we are doing with what is *actually* happening.
 ------------------------------------------------------------------------------*/

DECLARE_STATS_GROUP(TEXT("Driver Monitor"),STATGROUP_DriverMonitor, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Driver Monitor (AMD specific)"),STATGROUP_DriverMonitorAMD, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Driver Monitor (Intel specific)"),STATGROUP_DriverMonitorIntel, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Driver Monitor (Nvidia specific)"),STATGROUP_DriverMonitorNvidia, STATCAT_Advanced);

DECLARE_FLOAT_COUNTER_STAT(TEXT("Device Utilization %"),STAT_DriverMonitorDeviceUtilisation,STATGROUP_DriverMonitor);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Device Utilization % at cur p-state"),STAT_DM_I_DeviceUtilisationAtPState,STATGROUP_DriverMonitorIntel);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Device Unit 0 Utilization %"),STAT_DM_I_Device0Utilisation,STATGROUP_DriverMonitorIntel);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Device Unit 1 Utilization %"),STAT_DM_I_Device1Utilisation,STATGROUP_DriverMonitorIntel);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Device Unit 2 Utilization %"),STAT_DM_I_Device2Utilisation,STATGROUP_DriverMonitorIntel);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Device Unit 3 Utilization %"),STAT_DM_I_Device3Utilisation,STATGROUP_DriverMonitorIntel);

DECLARE_MEMORY_STAT(TEXT("VRAM Used Bytes"),STAT_DriverMonitorVRAMUsedBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("VRAM Free Bytes"),STAT_DriverMonitorVRAMFreeBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("VRAM Largest Free Bytes"),STAT_DriverMonitorVRAMLargestFreeBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("In Use Vid Mem Bytes"),STAT_DriverMonitorInUseVidMemBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("In Use Sys Mem Bytes"),STAT_DriverMonitorInUseSysMemBytes,STATGROUP_DriverMonitor);

DECLARE_MEMORY_STAT(TEXT("DMA Used Bytes"),STAT_DriverMonitorgartUsedBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("DMA Free Bytes"),STAT_DriverMonitorgartFreeBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("DMA Bytes"),STAT_DriverMonitorgartSizeBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("DMA Data Mapped"),STAT_DriverMonitorgartMapInBytesPerSample,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("DMA Data Unmapped"),STAT_DriverMonitorgartMapOutBytesPerSample,STATGROUP_DriverMonitor);

DECLARE_MEMORY_STAT(TEXT("Texture Page-off Bytes"),STAT_DriverMonitortexturePageOutBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("Texture Read-off Bytes"),STAT_DriverMonitortextureReadOutBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("Texture Volunteer Unload Bytes"),STAT_DriverMonitortextureVolunteerUnloadBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("AGP Texture Creation Bytes"),STAT_DriverMonitoragpTextureCreationBytes,STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("AGP Texture Creation Count"),STAT_DriverMonitoragpTextureCreationCount,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("AGP Ref Texture Creation Bytes"),STAT_DriverMonitoragprefTextureCreationBytes,STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("AGP Ref Texture Creation Count"),STAT_DriverMonitoragprefTextureCreationCount,STATGROUP_DriverMonitor);

DECLARE_MEMORY_STAT(TEXT("IOSurface Page-In Bytes"),STAT_DriverMonitorioSurfacePageInBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("IOSurface Page-Out Bytes"),STAT_DriverMonitorioSurfacePageOutBytes,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("IOSurface Read-Out Bytes"),STAT_DriverMonitorioSurfaceReadOutBytes,STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("IOSurface Texture Creation Count"),STAT_DriverMonitoriosurfaceTextureCreationCount,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("IOSurface Texture Creation Bytes"),STAT_DriverMonitoriosurfaceTextureCreationBytes,STATGROUP_DriverMonitor);

DECLARE_MEMORY_STAT(TEXT("OOL Texture Page-In Bytes"),STAT_DriverMonitoroolTexturePageInBytes,STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("OOL Texture Creation Count"),STAT_DriverMonitoroolTextureCreationCount,STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("OOL Texture Creation Bytes"),STAT_DriverMonitoroolTextureCreationBytes,STATGROUP_DriverMonitor);

DECLARE_MEMORY_STAT(TEXT("orphanedNonReusableSysMemoryBytes"),STAT_DriverMonitororphanedNonReusableSysMemoryBytes, STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("orphanedNonReusableSysMemoryCount"),STAT_DriverMonitororphanedNonReusableSysMemoryCount, STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("orphanedReusableSysMemoryBytes"),STAT_DriverMonitororphanedReusableSysMemoryBytes, STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("orphanedReusableSysMemoryCount"),STAT_DriverMonitororphanedReusableSysMemoryCount, STATGROUP_DriverMonitor);
DECLARE_FLOAT_COUNTER_STAT(TEXT("orphanedReusableSysMemoryHitRate"),STAT_DriverMonitororphanedReusableSysMemoryHitRate, STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("orphanedNonReusableVidMemoryBytes"),STAT_DriverMonitororphanedNonReusableVidMemoryBytes, STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("orphanedNonReusableVidMemoryCount"),STAT_DriverMonitororphanedNonReusableVidMemoryCount, STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("orphanedReusableVidMemoryBytes"),STAT_DriverMonitororphanedReusableVidMemoryBytes, STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("orphanedReusableVidMemoryCount"),STAT_DriverMonitororphanedReusableVidMemoryCount, STATGROUP_DriverMonitor);
DECLARE_FLOAT_COUNTER_STAT(TEXT("orphanedReusableVidMemoryHitRate"),STAT_DriverMonitororphanedReusableVidMemoryHitRate, STATGROUP_DriverMonitor);

DECLARE_MEMORY_STAT(TEXT("stdTextureCreationBytes"),STAT_DriverMonitorstdTextureCreationBytes, STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("stdTextureCreationCount"),STAT_DriverMonitorstdTextureCreationCount, STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("stdTexturePageInBytes"),STAT_DriverMonitorstdTexturePageInBytes, STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("surfaceBufferPageInBytes"),STAT_DriverMonitorsurfaceBufferPageInBytes, STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("surfaceBufferPageOutBytes"),STAT_DriverMonitorsurfaceBufferPageOutBytes, STATGROUP_DriverMonitor);
DECLARE_MEMORY_STAT(TEXT("surfaceBufferReadOutBytes"),STAT_DriverMonitorsurfaceBufferReadOutBytes, STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("surfaceTextureCreationCount"),STAT_DriverMonitorsurfaceTextureCreationCount, STATGROUP_DriverMonitor);

DECLARE_CYCLE_STAT(TEXT("CPU Wait For GPU"),STAT_DriverMonitorCPUWaitForGPU,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to Submit Commands"),STAT_DriverMonitorCPUWaitToSubmit,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to perform Surface Read"),STAT_DriverMonitorCPUWaitToSurfaceRead,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to perform Surface Resize"),STAT_DriverMonitorCPUWaitToSurfaceResize,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to perform Surface Write"),STAT_DriverMonitorCPUWaitToSurfaceWrite,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to perform VRAM Surface page-off"),STAT_DriverMonitorCPUWaitToSurfacePageOff,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to perform VRAM Surface page-on"),STAT_DriverMonitorCPUWaitToSurfacePageOn,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to reclaim Surface GART Backing Store"),STAT_DriverMonitorCPUWaitToReclaimSurfaceGART,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to perform VRAM Eviction"),STAT_DriverMonitorCPUWaitToVRAMEvict,STATGROUP_DriverMonitor);
DECLARE_CYCLE_STAT(TEXT("CPU Wait to free Data Buffer"),STAT_DriverMonitorCPUWaitToFreeDataBuffer,STATGROUP_DriverMonitor);

DECLARE_DWORD_COUNTER_STAT(TEXT("surfaceCount"), STAT_DriverMonitorSurfaceCount, STATGROUP_DriverMonitor);
DECLARE_DWORD_COUNTER_STAT(TEXT("textureCount"), STAT_DriverMonitorTextureCount, STATGROUP_DriverMonitor);

DECLARE_FLOAT_COUNTER_STAT(TEXT("GPU Core Utilization"), STAT_DM_NV_GPUCoreUtilization, STATGROUP_DriverMonitorNvidia);
DECLARE_FLOAT_COUNTER_STAT(TEXT("GPU Memory Utilization"), STAT_DM_NV_GPUMemoryUtilization, STATGROUP_DriverMonitorNvidia);

DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel C0 | Commands Completed"), STAT_DM_AMD_HWChannelC0Complete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel C0 | Commands Submitted"), STAT_DM_AMD_HWChannelC0Submit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel C1 | Commands Completed"), STAT_DM_AMD_HWChannelC1Complete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel C1 | Commands Submitted"), STAT_DM_AMD_HWChannelC1Submit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel DMA0 | Commands Completed"), STAT_DM_AMD_HWChannelDMA0Complete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel DMA0 | Commands Submitted"), STAT_DM_AMD_HWChannelDMA0Submit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel DMA1 | Commands Completed"), STAT_DM_AMD_HWChannelDMA1Complete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel DMA1 | Commands Submitted"), STAT_DM_AMD_HWChannelDMA1Submit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel GFX | Commands Completed"), STAT_DM_AMD_HWChannelGFXComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel GFX | Commands Submitted"), STAT_DM_AMD_HWChannelGFXSubmit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel SPU | Commands Completed"), STAT_DM_AMD_HWChannelSPUComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel SPU | Commands Submitted"), STAT_DM_AMD_HWChannelSPUSubmit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel UVD | Commands Completed"), STAT_DM_AMD_HWChannelUVDComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel UVD | Commands Submitted"), STAT_DM_AMD_HWChannelUVDSubmit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel VCE | Commands Completed"), STAT_DM_AMD_HWChannelVCEComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel VCE | Commands Submitted"), STAT_DM_AMD_HWChannelVCESubmit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel VCELLQ | Commands Completed"), STAT_DM_AMD_HWChannelVCELLQComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel VCELLQ | Commands Submitted"), STAT_DM_AMD_HWChannelVCELLQSubmit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel KIQ | Commands Completed"), STAT_DM_AMD_HWChannelKIQComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel KIQ | Commands Submitted"), STAT_DM_AMD_HWChannelKIQSubmit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel SAMU GPCOM | Commands Completed"), STAT_DM_AMD_HWChannelSAMUGPUCOMComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel SAMU GPCOM | Commands Submitted"), STAT_DM_AMD_HWChannelSAMUGPUCOMSubmit, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel SAMU RBI | Commands Completed"), STAT_DM_AMD_HWChannelSAMURBIComplete, STATGROUP_DriverMonitorAMD);
DECLARE_DWORD_COUNTER_STAT(TEXT("HWChannel SAMU RBI | Commands Submitted"), STAT_DM_AMD_HWChannelSAMURBISubmit, STATGROUP_DriverMonitorAMD);


template<typename T>
T GetMacGPUStat(TMap<FString, float> const& Stats, FString StatName)
{
	T Result = (T)0;
	if(Stats.Contains(StatName))
	{
		Result = Stats.FindRef(StatName);
	}
	return Result;
}

void FMacPlatformMisc::UpdateDriverMonitorStatistics(int32 DeviceIndex)
{
	if (DeviceIndex >= 0)
	{
		TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
		if (DeviceIndex < GPUs.Num())
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[DeviceIndex];
			TMap<FString, float> Stats = GPU.GetPerformanceStatistics();
			
			float DeviceUtil = GetMacGPUStat<float>(Stats, TEXT("Device Utilization %"));
			SET_FLOAT_STAT(STAT_DriverMonitorDeviceUtilisation, DeviceUtil);
			
			DeviceUtil = GetMacGPUStat<float>(Stats, TEXT("Device Utilization % at cur p-state"));
			SET_FLOAT_STAT(STAT_DM_I_DeviceUtilisationAtPState, DeviceUtil);
			
			DeviceUtil = GetMacGPUStat<float>(Stats, TEXT("Device Unit 0 Utilization %"));
			SET_FLOAT_STAT(STAT_DM_I_Device0Utilisation, DeviceUtil);
			
			DeviceUtil = GetMacGPUStat<float>(Stats, TEXT("Device Unit 1 Utilization %"));
			SET_FLOAT_STAT(STAT_DM_I_Device1Utilisation, DeviceUtil);
			
			DeviceUtil = GetMacGPUStat<float>(Stats, TEXT("Device Unit 2 Utilization %"));
			SET_FLOAT_STAT(STAT_DM_I_Device2Utilisation, DeviceUtil);
			
			DeviceUtil = GetMacGPUStat<float>(Stats, TEXT("Device Unit 3 Utilization %"));
			SET_FLOAT_STAT(STAT_DM_I_Device3Utilisation, DeviceUtil);
			
			int64 Memory = GetMacGPUStat<int64>(Stats, TEXT("vramUsedBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorVRAMUsedBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("vramFreeBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorVRAMFreeBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("vramLargestFreeBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorVRAMLargestFreeBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("inUseVidMemoryBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorInUseVidMemBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("inUseSysMemoryBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorInUseSysMemBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("gartSizeBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorgartSizeBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("gartFreeBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorgartFreeBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("gartUsedBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorgartUsedBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("gartMapInBytesPerSample"));
			SET_MEMORY_STAT(STAT_DriverMonitorgartMapInBytesPerSample, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("gartMapOutBytesPerSample"));
			SET_MEMORY_STAT(STAT_DriverMonitorgartMapOutBytesPerSample, Memory);
			
			int64 Cycles = GetMacGPUStat<int64>(Stats, TEXT("hardwareWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitForGPU, Cycles);
			
			Cycles = GetMacGPUStat<int64>(Stats, TEXT("hardwareSubmitWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitToSubmit, Cycles);
			
			Cycles = GetMacGPUStat<int64>(Stats, TEXT("surfaceReadLockIdleWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitToSurfaceRead, Cycles);
			
			Cycles = GetMacGPUStat<int64>(Stats, TEXT("surfaceCopyOutWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitToSurfacePageOff, Cycles);
			
			Cycles = GetMacGPUStat<int64>(Stats, TEXT("surfaceCopyInWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitToSurfacePageOn, Cycles);
			
			Cycles = GetMacGPUStat<int64>(Stats, TEXT("freeSurfaceBackingWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitToReclaimSurfaceGART, Cycles);
			
			Cycles = GetMacGPUStat<int64>(Stats, TEXT("vramEvictionWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitToVRAMEvict, Cycles);
			
			Cycles = GetMacGPUStat<int64>(Stats, TEXT("freeDataBufferWaitTime"));
			SET_CYCLE_COUNTER(STAT_DriverMonitorCPUWaitToFreeDataBuffer, Cycles);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("texturePageOutBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitortexturePageOutBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("textureReadOutBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitortextureReadOutBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("textureVolunteerUnloadBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitortextureVolunteerUnloadBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("agpTextureCreationBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitoragpTextureCreationBytes, Memory);
			
			uint32 Dword = GetMacGPUStat<int32>(Stats, TEXT("agpTextureCreationCount"));
			SET_DWORD_STAT(STAT_DriverMonitoragpTextureCreationCount, Dword);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("agprefTextureCreationBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitoragprefTextureCreationBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("agprefTextureCreationCount"));
			SET_DWORD_STAT(STAT_DriverMonitoragprefTextureCreationCount, Dword);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("ioSurfacePageInBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorioSurfacePageInBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("ioSurfacePageOutBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorioSurfacePageOutBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("ioSurfaceReadOutBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorioSurfaceReadOutBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("iosurfaceTextureCreationBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitoriosurfaceTextureCreationBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("iosurfaceTextureCreationCount"));
			SET_DWORD_STAT(STAT_DriverMonitoriosurfaceTextureCreationCount, Dword);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("oolTextureCreationBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitoroolTextureCreationBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("oolTexturePageInBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitoroolTexturePageInBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("oolTextureCreationCount"));
			SET_DWORD_STAT(STAT_DriverMonitoroolTextureCreationCount, Dword);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("orphanedNonReusableSysMemoryBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitororphanedNonReusableSysMemoryBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("orphanedNonReusableSysMemoryCount"));
			SET_DWORD_STAT(STAT_DriverMonitororphanedNonReusableSysMemoryCount, Dword);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("orphanedReusableSysMemoryBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitororphanedReusableSysMemoryBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("orphanedReusableSysMemoryCount"));
			SET_DWORD_STAT(STAT_DriverMonitororphanedReusableSysMemoryCount, Dword);
			
			float HitRate = GetMacGPUStat<float>(Stats, TEXT("orphanedReusableSysMemoryHitRate"));
			SET_FLOAT_STAT(STAT_DriverMonitororphanedReusableSysMemoryHitRate, HitRate);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("orphanedNonReusableVidMemoryBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitororphanedNonReusableVidMemoryBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("orphanedNonReusableVidMemoryCount"));
			SET_DWORD_STAT(STAT_DriverMonitororphanedNonReusableVidMemoryCount, Dword);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("orphanedReusableVidMemoryBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitororphanedReusableVidMemoryBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("orphanedReusableVidMemoryCount"));
			SET_DWORD_STAT(STAT_DriverMonitororphanedReusableVidMemoryCount, Dword);
			
			HitRate = GetMacGPUStat<float>(Stats, TEXT("orphanedReusableVidMemoryHitRate"));
			SET_FLOAT_STAT(STAT_DriverMonitororphanedReusableVidMemoryHitRate, HitRate);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("stdTextureCreationBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorstdTextureCreationBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("stdTextureCreationCount"));
			SET_DWORD_STAT(STAT_DriverMonitorstdTextureCreationCount, Dword);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("stdTexturePageInBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorstdTexturePageInBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("surfaceBufferPageInBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorsurfaceBufferPageInBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("surfaceBufferPageOutBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorsurfaceBufferPageOutBytes, Memory);
			
			Memory = GetMacGPUStat<int64>(Stats, TEXT("surfaceBufferReadOutBytes"));
			SET_MEMORY_STAT(STAT_DriverMonitorsurfaceBufferReadOutBytes, Memory);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("surfaceTextureCreationCount"));
			SET_DWORD_STAT(STAT_DriverMonitorsurfaceTextureCreationCount, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("surfaceCount"));
			SET_DWORD_STAT(STAT_DriverMonitorSurfaceCount, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("textureCount"));
			SET_DWORD_STAT(STAT_DriverMonitorTextureCount, Dword);
			
			HitRate = GetMacGPUStat<float>(Stats, TEXT("GPU Core Utilization"));
			SET_FLOAT_STAT(STAT_DM_NV_GPUCoreUtilization, HitRate);
			
			HitRate = GetMacGPUStat<float>(Stats, TEXT("GPU Memory Utilization"));
			SET_FLOAT_STAT(STAT_DM_NV_GPUMemoryUtilization, HitRate);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel C0 | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelC0Complete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel C0 | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelC0Submit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel C1 | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelC1Complete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel C1 | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelC1Submit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel DMA0 | Commands Completed"));
			if(!Dword)
			{
				Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel sDMA0 | Commands Completed"));
			}
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelDMA0Complete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel DMA0 | Commands Submitted"));
			if(!Dword)
			{
				Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel sDMA0 | Commands Submitted"));
			}
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelDMA0Submit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel DMA1 | Commands Completed"));
			if(!Dword)
			{
				Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel sDMA1 | Commands Completed"));
			}
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelDMA1Complete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel DMA1 | Commands Submitted"));
			if (!Dword)
			{
				Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel sDMA1 | Commands Submitted"));
			}
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelDMA1Submit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel GFX | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelGFXComplete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel GFX | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelGFXSubmit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel SPU | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelSPUComplete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel SPU | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelSPUSubmit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel UVD | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelUVDComplete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel UVD | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelUVDSubmit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel VCE | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelVCEComplete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel VCE | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelVCESubmit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel VCELLQ | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelVCELLQComplete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel VCELLQ | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelVCELLQSubmit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel KIQ | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelKIQComplete, Dword);

			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel KIQ | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelKIQSubmit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel SAMU GPCOM | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelSAMUGPUCOMComplete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel SAMU GPCOM | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelSAMUGPUCOMSubmit, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel SAMU RBI | Commands Completed"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelSAMURBIComplete, Dword);
			
			Dword = GetMacGPUStat<int32>(Stats, TEXT("HWChannel SAMU RBI | Commands Submitted"));
			SET_DWORD_STAT(STAT_DM_AMD_HWChannelSAMURBISubmit, Dword);
		}
	}
}

int FMacPlatformMisc::GetDefaultStackSize()
{
	// Thread sanitiser requires 5x the memory.
#if __has_feature(thread_sanitizer)
	return 20 * 1024 * 1024;
#else
	return 4 * 1024 * 1024;
#endif
}
