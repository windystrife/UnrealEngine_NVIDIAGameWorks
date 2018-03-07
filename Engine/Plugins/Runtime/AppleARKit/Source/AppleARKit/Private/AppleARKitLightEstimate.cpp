// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// AppleARKit
#include "AppleARKitLightEstimate.h"


#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

FAppleARKitLightEstimate::FAppleARKitLightEstimate( ARLightEstimate* InARLightEstimate )
{
	bIsValid = InARLightEstimate != nullptr;
	AmbientIntensity = InARLightEstimate != nullptr ? InARLightEstimate.ambientIntensity : 0.0f;
}

#endif // #ARKIT_SUPPORT
