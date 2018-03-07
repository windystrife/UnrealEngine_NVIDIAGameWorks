// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=================================================================================
 MacPlatformSurvey.mm: Mac OS X platform hardware-survey classes
 =================================================================================*/

#include "MacPlatformSurvey.h"
#include "UnrealString.h"
#include "SynthBenchmark.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "CoreGlobals.h"

#import <IOKit/graphics/IOGraphicsLib.h>
#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/ps/IOPSKeys.h>
#include <OpenGL/gl3.h>

namespace MacPlatformSurveDefs
{
	static const double SurveyTimeoutSeconds = 300.0;
	static const float WaitSleepSeconds = 2.0;
}

bool FMacPlatformSurvey::bSurveyPending = false;
bool FMacPlatformSurvey::bSurveyComplete = false;
bool FMacPlatformSurvey::bSurveyFailed = false;
double FMacPlatformSurvey::SurveyStartTimeSeconds = 0.0;
FHardwareSurveyResults FMacPlatformSurvey::Results;

bool FMacPlatformSurvey::GetSurveyResults( FHardwareSurveyResults& OutResults, bool bWait )
{
	// Early out failed state
	if (bSurveyFailed)
	{
		return false;
	}

	if (!bSurveyComplete)
	{
		// Tick survey process
		double StartWaitTime = FPlatformTime::Seconds();
		do
		{
			if (!bSurveyPending)
			{
				// start survey
				BeginSurveyHardware();
			}
			else
			{
				// tick pending survey
				TickSurveyHardware(Results);
			}

			if (bWait && bSurveyPending)
			{
				FPlatformProcess::Sleep(MacPlatformSurveDefs::WaitSleepSeconds);
			}

		} while (bWait && bSurveyPending);
	}

	if (bSurveyComplete)
	{
		OutResults = Results;
	}
	return bSurveyComplete;
}

void FMacPlatformSurvey::BeginSurveyHardware()
{
	if (bSurveyPending)
	{
		UE_LOG(LogMac, Error, TEXT("FMacPlatformSurvey::BeginSurveyHardware() survey already in-progress") );
		bSurveyFailed = true;
		return;
	}

	SurveyStartTimeSeconds = FPlatformTime::Seconds();
	bSurveyPending = true;
}

void FMacPlatformSurvey::TickSurveyHardware( FHardwareSurveyResults& OutResults )
{
	if (!bSurveyPending)
	{
		bSurveyFailed = true;
		return;
	}

	if (MacPlatformSurveDefs::SurveyTimeoutSeconds < FPlatformTime::Seconds() - SurveyStartTimeSeconds)
	{
		UE_LOG(LogMac, Error, TEXT("FMacPlatformSurvey::EndSurveyHardware() survey timed out") );
		bSurveyPending = false;
		bSurveyFailed = true;
		return;
	}

	FMemory::Memset(&OutResults, 0, sizeof(FHardwareSurveyResults));

	bSurveyPending = false;

	WriteFStringToResults(OutResults.Platform, TEXT("Mac"));

	// Get memory
	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	uint64_t FreeMem = Stats.free_count * PageSize;
	uint64_t UsedMem = (Stats.active_count + Stats.inactive_count + Stats.wire_count) * PageSize;
	uint64_t TotalPhys = FreeMem + UsedMem;
	OutResults.MemoryMB = uint32(float(TotalPhys/1024.0/1024.0)+ .1f);

	if ([NSOpenGLContext currentContext])
	{
		// Get OpenGL version
		GLint MajorVersion = 0;
		GLint MinorVersion = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &MajorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &MinorVersion);
		FString OpenGLVerString = FString::Printf(TEXT("%d.%d"), MajorVersion, MinorVersion);
		WriteFStringToResults(OutResults.MultimediaAPI, OpenGLVerString);

		// Display devices
		OutResults.DisplayCount = 0;
		CGDirectDisplayID Displays[FHardwareSurveyResults::MaxDisplayCount];
		if (CGGetActiveDisplayList(FHardwareSurveyResults::MaxDisplayCount, Displays, &OutResults.DisplayCount) == CGDisplayNoErr)
		{
			for (uint32 Index = 0; Index < OutResults.DisplayCount; Index++)
			{
				FHardwareDisplay& Display = OutResults.Displays[Index];

				Display.CurrentModeWidth = CGDisplayPixelsWide(Displays[Index]);
				Display.CurrentModeHeight = CGDisplayPixelsHigh(Displays[Index]);

				WriteFStringToResults(Display.GPUCardName, FString(StringCast<TCHAR>((const char*)glGetString(GL_RENDERER)).Get()));
				WriteFStringToResults(Display.GPUDriverVersion, FString(StringCast<TCHAR>((const char*)glGetString(GL_VERSION)).Get()));

				CGLRendererInfoObj InfoObject;
				GLint NumRenderers = 0;
				if (CGLQueryRendererInfo(CGDisplayIDToOpenGLDisplayMask(Displays[Index]), &InfoObject, &NumRenderers) == kCGLNoError && NumRenderers > 0)
				{
					CGLDescribeRenderer(InfoObject, 0, kCGLRPVideoMemoryMegabytes, (GLint*)&Display.GPUDedicatedMemoryMB);
					CGLDestroyRendererInfo(InfoObject);
				}
				else
				{
					UE_LOG(LogMac, Warning, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to query renderer info for disaplay %u"), Index );
					OutResults.ErrorCount++;
					WriteFStringToResults(OutResults.LastSurveyError, TEXT("Failed to query renderer info"));
					WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
				}
			}
		}
		else
		{
			UE_LOG(LogMac, Warning, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to get active displays list") );
			OutResults.ErrorCount++;
			WriteFStringToResults(OutResults.LastSurveyError, TEXT("Failed to get active displays list"));
			WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
		}

		if (OutResults.DisplayCount == 0)
		{
			OutResults.ErrorCount++;
			WriteFStringToResults(OutResults.LastSurveyError, TEXT("Display count zero"));
			WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
		}
		else if (OutResults.DisplayCount > 3)
		{
			OutResults.ErrorCount++;
			WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Display count %d"), OutResults.DisplayCount));
			WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
		}
	}

	// Get CPU count
	int32 NumCPUs = 1;
	size_t Size = sizeof(int32);
	if (sysctlbyname("hw.ncpu", &NumCPUs, &Size, NULL, 0) == 0)
	{
		OutResults.CPUCount = NumCPUs;
	}
	else
	{
		OutResults.CPUCount = 0;
	}

	ISynthBenchmark::Get().Run(OutResults.SynthBenchmark, true, 5.f);

	// Get CPU speed
	if (OutResults.CPUCount > 0)
	{
		int64 CPUSpeed = 0;
		Size = sizeof(int64);
		if (sysctlbyname("hw.cpufrequency", &CPUSpeed, &Size, NULL, 0) == 0)
		{
			OutResults.CPUClockGHz = 0.000000001 * CPUSpeed;
		}
		else
		{
			OutResults.ErrorCount++;
			WriteFStringToResults(OutResults.LastSurveyError, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to get processor speed from sysctlbyname()"));
			WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
		}
	}
	else
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to get processor count from sysctlbyname()"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	// Get CPU brand
	FString CPUBrand = FMacPlatformMisc::GetCPUVendor();
	WriteFStringToResults(OutResults.CPUBrand, CPUBrand);
	if (CPUBrand.Len() == 0)
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to get processor brand from FMacPlatformMisc::GetCPUVendor()"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	// Get CPU name
	ANSICHAR TempANSICHARBuffer[FHardwareSurveyResults::MaxStringLength];
	Size = sizeof(TempANSICHARBuffer);
	if (sysctlbyname("machdep.cpu.brand_string", TempANSICHARBuffer, &Size, NULL, 0) == 0)
	{
		WriteFStringToResults(OutResults.CPUNameString, FString(StringCast<TCHAR>(&TempANSICHARBuffer[0]).Get()));
	}
	else
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to get processor name from sysctlbyname()"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	// Get CPU info
	OutResults.CPUInfo = FMacPlatformMisc::GetCPUInfo();

	// Get HDD details
	OutResults.HardDriveGB = -1;
	NSDictionary *HDDAttributes = [[NSFileManager defaultManager] attributesOfFileSystemForPath: @"/" error: nil];
	if (HDDAttributes)
	{
		OutResults.HardDriveGB = (uint32)([[HDDAttributes objectForKey: NSFileSystemFreeSize] longLongValue] / 1024 / 1024 / 1024);
	}
	else
	{
		UE_LOG(LogMac, Warning, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to get root-folder drive size") );
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("attributesOfFileSystemForPath failed"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	// OS info
	FString OSXVersion, OSXBuild;
	FMacPlatformMisc::GetOSVersions(OSXVersion, OSXBuild);
	WriteFStringToResults(OutResults.OSVersion, FString(TEXT("Mac OS X ")) + OSXVersion);
	WriteFStringToResults(OutResults.OSSubVersion, OSXBuild);
	OutResults.OSBits = FPlatformMisc::Is64bitOperatingSystem() ? 64 : 32;

	// OS language
	NSArray* Languages = [[NSUserDefaults standardUserDefaults] objectForKey: @"AppleLanguages"];
	NSString* PreferredLang = [Languages objectAtIndex: 0];
	FPlatformString::CFStringToTCHAR((CFStringRef)PreferredLang, OutResults.OSLanguage);

	// Get system power info to determine whether we're running on a laptop or desktop computer
	OutResults.bIsLaptopComputer = false;
	CFTypeRef PowerSourcesInfo = IOPSCopyPowerSourcesInfo();
	if (PowerSourcesInfo)
	{
		CFArrayRef PowerSourcesArray = IOPSCopyPowerSourcesList(PowerSourcesInfo);
		for (CFIndex Index = 0; Index < CFArrayGetCount(PowerSourcesArray); Index++)
		{
			CFTypeRef PowerSource = CFArrayGetValueAtIndex(PowerSourcesArray, Index);
			NSDictionary* Description = (NSDictionary*)IOPSGetPowerSourceDescription(PowerSourcesInfo, PowerSource);
			if ([(NSString*)[Description objectForKey: @kIOPSTypeKey] isEqualToString: @kIOPSInternalBatteryType])
			{
				OutResults.bIsLaptopComputer = true;
				break;
			}
		}
		CFRelease(PowerSourcesArray);
		CFRelease(PowerSourcesInfo);
	}
	else
	{
		UE_LOG(LogMac, Warning, TEXT("FMacPlatformSurvey::TickSurveyHardware() failed to get system power sources info. Assuming desktop Mac.") );
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("IOPSCopyPowerSourcesInfo() failed to get system power sources info"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	bSurveyComplete = true;
}

void FMacPlatformSurvey::WriteFStringToResults(TCHAR* OutBuffer, const FString& InString)
{
	FMemory::Memset( OutBuffer, 0, sizeof(TCHAR) * FHardwareSurveyResults::MaxStringLength );
	TCHAR* Cursor = OutBuffer;
	for (int32 i = 0; i < FMath::Min(InString.Len(), FHardwareSurveyResults::MaxStringLength - 1); i++)
	{
		*Cursor++ = InString[i];
	}
}
