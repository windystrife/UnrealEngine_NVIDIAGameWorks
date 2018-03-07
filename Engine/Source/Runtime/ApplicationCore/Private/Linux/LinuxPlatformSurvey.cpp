// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformSurvey.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"

#include "SynthBenchmark.h"

bool FLinuxPlatformSurvey::GetSurveyResults(FHardwareSurveyResults& OutResults, bool bWait)
{
	FMemory::Memset(&OutResults, 0, sizeof(FHardwareSurveyResults));
	WriteFStringToResults(OutResults.Platform, TEXT("Linux"));

	// CPU
	OutResults.CPUCount = FPlatformMisc::NumberOfCores();  // TODO [RCL] 2015-07-15: parse /proc/cpuinfo for GHz/brand

	// Memory
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	OutResults.MemoryMB = MemoryConstants.TotalPhysical / ( 1024ULL * 1024ULL );

	// Misc
	OutResults.bIsRemoteSession = FPlatformMisc::HasBeenStartedRemotely();
	OutResults.bIsLaptopComputer = FPlatformMisc::IsRunningOnBattery();	// FIXME [RCL] 2015-07-15: incorrect. Laptops don't have to run on battery

	// Synth benchmark
	ISynthBenchmark::Get().Run(OutResults.SynthBenchmark, true, 5.f);

	OutResults.ErrorCount++;
	WriteFStringToResults(OutResults.LastSurveyError, TEXT("Survey is incomplete"));
	WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT("CPU, OS details are missing"));

	return true;
}

void FLinuxPlatformSurvey::GetOSName(FHardwareSurveyResults& OutResults)
{
	// TODO [RCL] 2015-07-15: check if /etc/os-release or /etc/redhat-release exist and parse it

	/*
	TArray<FString> OsReleaseLines;
	if( FFileHelper::LoadANSITextFileToStrings(TEXT("/etc/os-release"), &IFileManager::Get(), OsReleaseLines))
	{
		//...
	}
	*/
}

void FLinuxPlatformSurvey::WriteFStringToResults(TCHAR* OutBuffer, const FString& InString)
{
	FMemory::Memset( OutBuffer, 0, sizeof(TCHAR) * FHardwareSurveyResults::MaxStringLength );
	TCHAR* Cursor = OutBuffer;
	for (int32 i = 0; i < FMath::Min(InString.Len(), FHardwareSurveyResults::MaxStringLength - 1); i++)
	{
		*Cursor++ = InString[i];
	}
}
