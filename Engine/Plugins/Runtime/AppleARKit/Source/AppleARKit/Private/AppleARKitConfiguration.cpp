// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// AppleARKit
#include "AppleARKitConfiguration.h"
#include "AppleARKitModule.h"

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

ARWorldAlignment ToARWorldAlignment( const EAppleARKitWorldAlignment& InWorldAlignment )
{
	switch ( InWorldAlignment )
	{
		case EAppleARKitWorldAlignment::Gravity:
			return ARWorldAlignmentGravity;
			
    	case EAppleARKitWorldAlignment::GravityAndHeading:
    		return ARWorldAlignmentGravityAndHeading;

    	case EAppleARKitWorldAlignment::Camera:
    		return ARWorldAlignmentCamera;
    };
};

ARPlaneDetection ToARPlaneDetection( const EAppleARKitPlaneDetection& InPlaneDetection )
{
	ARPlaneDetection PlaneDetection = ARPlaneDetectionNone;
    
    if ( !!(InPlaneDetection & EAppleARKitPlaneDetection::Horizontal) )
    {
    	PlaneDetection |= ARPlaneDetectionHorizontal;
    }

    return PlaneDetection;
}

ARConfiguration* FAppleARKitConfiguration::ToARConfiguration( const FAppleARKitConfiguration& InConfiguration )
{
	// If we support and have requested world tracking, make a configuration accordingly
	ARWorldTrackingConfiguration* WorldTrackingConfiguration = (InConfiguration.WorldTracking.IsSet())
		? [ARWorldTrackingConfiguration new]
		: nullptr;
	
	// If we have world tracking, use that configuration. Otherwise, use a simple (orientation-based) configration.
	ARConfiguration* SessionConfiguration = (WorldTrackingConfiguration != nullptr)
		? WorldTrackingConfiguration
		: [AROrientationTrackingConfiguration new];
	

	// Regular session setup
	{
		// Copy / convert properties
		SessionConfiguration.lightEstimationEnabled = InConfiguration.bLightEstimationEnabled;
		SessionConfiguration.providesAudioData = InConfiguration.bProvidesAudioData;
		SessionConfiguration.worldAlignment = ToARWorldAlignment(InConfiguration.Alignment);
	}
	
	// World Tracking setup, if available
	if ( InConfiguration.WorldTracking.IsSet() )
	{
		WorldTrackingConfiguration.planeDetection = ToARPlaneDetection(InConfiguration.WorldTracking->PlaneDetection);
	}
    
    return SessionConfiguration;
}

#endif
