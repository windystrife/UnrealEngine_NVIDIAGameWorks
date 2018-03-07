// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
//  main.m
//  UDKRemote
//
//  Created by jadams on 7/28/10.
//

#import <UIKit/UIKit.h>
#include "IPhoneObjCWrapper.h"

time_t GAppInvokeTime;
int GEngineVersion = 1;
uint64_t GStartupFreeMem;
uint64_t GStartupUsedMem;

int main(int argc, char *argv[])
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	// Save off the memory amounts at startup
	IPhoneGetPhysicalMemoryInfo(GStartupFreeMem, GStartupUsedMem);
	
	// Update the app statistics
	GAppInvokeTime = time(NULL);
	if (IPhoneLoadUserSettingU64("IPhoneHome::LastRunCrashed") != 0)
	{
		IPhoneIncrementUserSettingU64("IPhoneHome::NumCrashes");
	}
	else
	{
		IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 1);
	}
	IPhoneIncrementUserSettingU64("IPhoneHome::NumInvocations");

	// Start the GUI
	int retVal = UIApplicationMain(argc, argv, nil, nil);

    [pool release];
    return retVal;
}
