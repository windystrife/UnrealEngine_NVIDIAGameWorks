// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
IOSPlatformProcess.cpp: iOS implementations of Process functions
=============================================================================*/

#include "IOSPlatformProcess.h"
#include "ApplePlatformRunnableThread.h"
#include "IOSAppDelegate.h"
#include <mach-o/dyld.h>

// numbers recommended by Apple
#define GAME_THREAD_PRIORITY 47
#define RENDER_THREAD_PRIORITY 45


const TCHAR* FIOSPlatformProcess::ComputerName()
{
	static TCHAR Result[256]=TEXT("");

	if( !Result[0] )
	{
		ANSICHAR AnsiResult[ARRAY_COUNT(Result)];
		gethostname(AnsiResult, ARRAY_COUNT(Result));
		FCString::Strcpy(Result, ANSI_TO_TCHAR(AnsiResult));
	}
	return Result;
}

const TCHAR* FIOSPlatformProcess::BaseDir()
{
	return TEXT("");
}

FRunnableThread* FIOSPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadApple();
}

void FIOSPlatformProcess::LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	UE_LOG(LogIOS, Log,  TEXT("LaunchURL %s %s"), URL, Parms?Parms:TEXT("") );
	NSString* CFUrl = (NSString*)FPlatformString::TCHARToCFString( URL );
	bool Result = [[UIApplication sharedApplication] openURL: [NSURL URLWithString: CFUrl]];
	if (Error != nullptr)
	{
		*Error = Result ? TEXT("") : TEXT("unable to open url");
	}
}

bool FIOSPlatformProcess::CanLaunchURL(const TCHAR* URL)
{
	NSString* CFUrl = (NSString*)FPlatformString::TCHARToCFString(URL);
    bool Result = [[UIApplication sharedApplication] canOpenURL:[NSURL URLWithString: CFUrl]];
	return Result;
}

FString FIOSPlatformProcess::GetGameBundleId()
{
	return FString([[NSBundle mainBundle] bundleIdentifier]);
}

void FIOSPlatformProcess::SetRealTimeMode()
{
	if ([IOSAppDelegate GetDelegate].OSVersion < 7 && FPlatformMisc::NumberOfCores() > 1)
	{
		mach_timebase_info_data_t TimeBaseInfo;
		mach_timebase_info( &TimeBaseInfo );
		double MsToAbs = ((double)TimeBaseInfo.denom / (double)TimeBaseInfo.numer) * 1000000.0;
		uint32 NormalProcessingTimeMs = 20;
		uint32 ConstraintProcessingTimeMs = 60;

		thread_time_constraint_policy_data_t Policy;
		Policy.period      = 0;
		Policy.computation = (uint32_t)(NormalProcessingTimeMs * MsToAbs);
		Policy.constraint  = (uint32_t)(ConstraintProcessingTimeMs * MsToAbs);
		Policy.preemptible = true;

		thread_policy_set(pthread_mach_thread_np(pthread_self()), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&Policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
	}
}

// Set the game thread priority to very high, slightly above the render thread
void FIOSPlatformProcess::SetupThread(const int Priority)
{
	struct sched_param Sched;
	FMemory::Memzero(&Sched, sizeof(struct sched_param));

	// Read the current priority and policy
	int32 CurrentPolicy = SCHED_RR;
	pthread_getschedparam(pthread_self(), &CurrentPolicy, &Sched);

	// Set the new priority and policy (apple recommended FIFO for the two main non-working threads)
	int32 Policy = SCHED_FIFO;
	Sched.sched_priority = Priority;
	pthread_setschedparam(pthread_self(), Policy, &Sched);
}

void FIOSPlatformProcess::SetupGameThread()
{
	SetupThread(GAME_THREAD_PRIORITY);
}

void FIOSPlatformProcess::SetupRenderThread()
{
	SetupThread(RENDER_THREAD_PRIORITY);
}

void FIOSPlatformProcess::SetThreadAffinityMask(uint64 AffinityMask)
{
	if ([IOSAppDelegate GetDelegate].OSVersion >= 8 && FPlatformMisc::NumberOfCores() > 1)
	{
		thread_affinity_policy AP;
		AP.affinity_tag = AffinityMask;
		thread_policy_set(pthread_mach_thread_np(pthread_self()), THREAD_AFFINITY_POLICY, (integer_t*)&AP, THREAD_AFFINITY_POLICY_COUNT);
	}
	else
	{
		FGenericPlatformProcess::SetThreadAffinityMask(AffinityMask);
	}
}

const TCHAR* FIOSPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static TCHAR Result[512] = TEXT("");
	if (!Result[0])
	{
		NSString *NSExeName = [[[NSBundle mainBundle] executablePath] lastPathComponent];
		FPlatformString::CFStringToTCHAR((CFStringRef)NSExeName, Result);
	}
	return Result;
}

FString FIOSPlatformProcess::GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration)
{
    SCOPED_AUTORELEASE_POOL;
    
    FString PlatformName = TEXT("IOS");
    FString ExecutableName = AppName;
    if (BuildConfiguration != EBuildConfigurations::Development && BuildConfiguration != EBuildConfigurations::DebugGame)
    {
        ExecutableName += FString::Printf(TEXT("-%s-%s"), *PlatformName, EBuildConfigurations::ToString(BuildConfiguration));
    }
    
    NSURL* CurrentBundleURL = [[NSBundle mainBundle] bundleURL];
    NSString* CurrentBundleName = [[CurrentBundleURL lastPathComponent] stringByDeletingPathExtension];
    if(FString(CurrentBundleName) == ExecutableName)
    {
        CFStringRef FilePath = CFURLCopyFileSystemPath((CFURLRef)CurrentBundleURL, kCFURLPOSIXPathStyle);
        FString ExecutablePath = FString::Printf(TEXT("%s/%s"), *FString((NSString*)FilePath), *ExecutableName);
        CFRelease(FilePath);
        return ExecutablePath;
    }
    else
    {
        return FString();
    }
}

const uint64 FIOSPlatformAffinity::GetMainGameMask()
{
	return MAKEAFFINITYMASK1(0);
}

const uint64 FIOSPlatformAffinity::GetRenderingThreadMask()
{
	static int Mask = 0;
	if (Mask == 0)
	{
		switch (FPlatformMisc::NumberOfCores())
		{
		case 2:
		case 3:
			Mask = MAKEAFFINITYMASK1(1);
			break;
		default:
			Mask = FGenericPlatformAffinity::GetRenderingThreadMask();
			break;
		}
	}

	return Mask;
}

const uint64 FIOSPlatformAffinity::GetRTHeartBeatMask()
{
	static int Mask = 0;
	if (Mask == 0)
	{
		switch (FPlatformMisc::NumberOfCores())
		{
		case 2:
		case 3:
			Mask = MAKEAFFINITYMASK1(0);
			break;
		default:
			Mask = FGenericPlatformAffinity::GetRTHeartBeatMask();
			break;
		}
	}

	return Mask;
}

const uint64 FIOSPlatformAffinity::GetPoolThreadMask()
{
	static int Mask = 0;
	if (Mask == 0)
	{
		switch (FPlatformMisc::NumberOfCores())
		{
		case 2:
			Mask = MAKEAFFINITYMASK1(1);
			break;
		case 3:
			Mask = MAKEAFFINITYMASK1(2);
			break;
		default:
			Mask = FGenericPlatformAffinity::GetPoolThreadMask();
			break;
		}
	}

	return Mask;
}

const uint64 FIOSPlatformAffinity::GetTaskGraphThreadMask()
{
	static int Mask = 0;
	if (Mask == 0)
	{
		switch (FPlatformMisc::NumberOfCores())
		{
		case 2:
			Mask = MAKEAFFINITYMASK1(1);
			break;
		case 3:
			Mask = MAKEAFFINITYMASK1(2);
			break;
		default:
			Mask = FGenericPlatformAffinity::GetTaskGraphThreadMask();
			break;
		}
	}

	return Mask;
}

const uint64 FIOSPlatformAffinity::GetStatsThreadMask()
{
	static int Mask = 0;
	if (Mask == 0)
	{
		switch (FPlatformMisc::NumberOfCores())
		{
		case 2:
			Mask = MAKEAFFINITYMASK1(0);
			break;
		case 3:
			Mask = MAKEAFFINITYMASK1(2);
			break;
		default:
			Mask = FGenericPlatformAffinity::GetStatsThreadMask();
			break;
		}
	}

	return Mask;
}

const uint64 FIOSPlatformAffinity::GetNoAffinityMask()
{
	static int Mask = 0;
	if (Mask == 0)
	{
		switch (FPlatformMisc::NumberOfCores())
		{
		case 2:
		case 3:
			Mask = (1 << FPlatformMisc::NumberOfCores()) - 1;
			break;
		default:
			Mask = FGenericPlatformAffinity::GetNoAffinityMask();
			break;
		}
	}

	return Mask;
}

