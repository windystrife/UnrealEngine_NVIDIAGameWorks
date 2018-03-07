// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsTargetSettings.h"

/* UWindowsTargetSettings structors
 *****************************************************************************/

UWindowsTargetSettings::UWindowsTargetSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	MinimumOSVersion = EMinimumSupportedOS::MSOS_Vista;

	// Default windows settings
	AudioSampleRate = 48000;
	AudioCallbackBufferFrameSize = 1024;
	AudioNumBuffersToEnqueue = 1;
	AudioNumSourceWorkers = 4;
}
