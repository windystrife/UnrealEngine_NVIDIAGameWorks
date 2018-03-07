// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformSurvey.cpp: HardwareSurvey implementation
=============================================================================*/

#include "IOSPlatformSurvey.h"
#include "IOSAppDelegate.h"
#include "IOSWindow.h"

bool FIOSPlatformSurvey::GetSurveyResults(FHardwareSurveyResults& OutResults, bool bWait)
{
	FCString::Strcpy(OutResults.Platform, FPlatformMisc::GetDefaultDeviceProfileName());
	FCString::Strcpy(OutResults.OSVersion, *FString([[UIDevice currentDevice] systemVersion]));
#if PLATFORM_64BITS
	OutResults.OSBits = 64;
#else
	OutResults.OSBits = 32;
#endif
	FCString::Strcpy(OutResults.OSLanguage, *FString([[NSLocale preferredLanguages] objectAtIndex:0]));
	FCString::Strcpy(OutResults.MultimediaAPI, FPlatformMisc::HasPlatformFeature(TEXT("Metal")) ? TEXT("Metal") : TEXT("ES2"));
	OutResults.CPUCount = FPlatformMisc::NumberOfCores();

	// display 0 is max size
	CGRect MainFrame = [[UIScreen mainScreen] bounds];
	if ([IOSAppDelegate GetDelegate].OSVersion < 8.0f && ![IOSAppDelegate GetDelegate].bDeviceInPortraitMode)
	{
		Swap(MainFrame.size.width, MainFrame.size.height);
	}
	float Scale = [[UIScreen mainScreen] scale];
	OutResults.Displays[0].CurrentModeWidth = MainFrame.size.width * Scale;
	OutResults.Displays[0].CurrentModeHeight = MainFrame.size.height * Scale;

	// display 1 is current size
	FPlatformRect ScreenSize = FIOSWindow::GetScreenRect();
	OutResults.Displays[1].CurrentModeWidth = ScreenSize.Right - ScreenSize.Left;
	OutResults.Displays[1].CurrentModeHeight = ScreenSize.Bottom - ScreenSize.Top;
	return true;
}
