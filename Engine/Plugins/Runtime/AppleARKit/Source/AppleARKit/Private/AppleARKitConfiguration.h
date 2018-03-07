// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnumClassFlags.h"

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
#import <ARKit/ARKit.h>
#endif // ARKIT_SUPPORT

/**
 * Enum constants for indicating the world alignment.
 */
enum class EAppleARKitWorldAlignment : uint8
{
	/** Aligns the world with gravity that is defined by vector (0, -1, 0). */
	Gravity,
	
	/**
	 * Aligns the world with gravity that is defined by the vector (0, -1, 0)
	 * and heading (w.r.t. True North) that is given by the vector (0, 0, -1).
	 */
	GravityAndHeading,
	
	/** Aligns the world with the camera's orientation. */
	Camera
};
ENUM_CLASS_FLAGS(EAppleARKitWorldAlignment)



/**
 Option set indicating the type of planes to detect.
 */
enum class EAppleARKitPlaneDetection : uint8
{
	/** No plane detection is run. */
	None        = 1,
	
	/** Plane detection determines horizontal planes in the scene. */
	Horizontal  = 2
};
ENUM_CLASS_FLAGS(EAppleARKitPlaneDetection)


/**
 * An object to describe and configure the Augmented Reality techniques to be used in a
 * UAppleARKitSession.
 */
class APPLEARKIT_API FAppleARKitConfiguration
{
public:
	
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	static ARConfiguration* ToARConfiguration( const FAppleARKitConfiguration& InConfiguration );
#endif //ARKIT_SUPPORT

	/**
	 * Enable or disable light estimation.
	 * @discussion Enabled by default.
	 */
	bool bLightEstimationEnabled = true;
	
	/**
	 * Enables audio capture during the AR session
	 */
	bool bProvidesAudioData = false;
	
	/**
	 * The alignment that transforms will be with respect to.
	 * @discussion The default is ARWorldAlignmentGravity.
	 */
	EAppleARKitWorldAlignment Alignment = EAppleARKitWorldAlignment::Gravity;
	
	/**
	 * A session configuration for world tracking.
	 *
	 * World tracking provides 6 degrees of freedom tracking of the device.
	 * By finding feature points in the scene, world tracking enables performing hit-tests against the frame.
	 * Tracking can no longer be resumed once the session is paused.
	 */
	struct FWorldTracking
	{
		/**
		 * Plane detection settings.
		 */
		EAppleARKitPlaneDetection PlaneDetection = EAppleARKitPlaneDetection::Horizontal;
	};
	TOptional<FWorldTracking> WorldTracking = FWorldTracking();
};


