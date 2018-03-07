// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// UE4
#include "Math/Transform.h"
#include "Math/UnrealMathUtility.h"
#include "UnrealEngine.h"
#include "Engine/GameViewportClient.h"
#include "ARTrackingQuality.h"

// ARKit
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
#import <ARKit/ARKit.h>
#endif // ARKIT_SUPPORT

#include "AppleARKitCamera.generated.h"


enum class EAppleARKitBackgroundFitMode : uint8
{
	/** The background image will be letterboxed to fit the screen */
	Fit,

	/** The background will be scaled & cropped to the screen */
	Fill,

	/** The background image will be stretched to fill the screen */
	Stretch,
};


/**
 * A model representing the camera and its properties at a single point in time.
 */
USTRUCT( BlueprintType, Category="AppleARKit" )
struct APPLEARKIT_API FAppleARKitCamera
{
	GENERATED_BODY()
	
	// Default constructor
	FAppleARKitCamera() {};

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

	/** 
	 * This is a conversion copy-constructor that takes a raw ARCamera and fills this structs members
	 * with the UE4-ified versions of ARCamera's properties.
	 */ 
	FAppleARKitCamera( ARCamera* InARCamera );

#endif // #ARKIT_SUPPORT

	/**
	 * The tracking quality of the camera.
	 */
	UPROPERTY( BlueprintReadOnly, Category="AppleARKit|Camera" )
	EARTrackingQuality TrackingQuality;

	/**
	 * The transformation matrix that defines the camera's rotation and translation in world coordinates.
	 */
	UPROPERTY( BlueprintReadOnly, Category="AppleARKit|Camera" )
	FTransform Transform;

	/* Raw orientation of the camera */
	UPROPERTY(BlueprintReadOnly, Category = "AppleARKit|Camera")
	FQuat Orientation;

	/* Raw position of the camera */
	UPROPERTY(BlueprintReadOnly, Category = "AppleARKit|Camera")
	FVector Translation;

	/**
	 * Camera image resolution in pixels
	 */
	UPROPERTY( BlueprintReadOnly, Category="AppleARKit|Camera" )
	FVector2D ImageResolution;

	/**
	 * Camera focal length in pixels
	 */
	UPROPERTY( BlueprintReadOnly, Category="AppleARKit|Camera" )
	FVector2D FocalLength;

	/**
	 * Camera principal point in pixels
	 */
	UPROPERTY( BlueprintReadOnly, Category="AppleARKit|Camera" )
	FVector2D PrincipalPoint;

	/** Returns the ImageResolution aspect ration (Width / Height) */
	float GetAspectRatio() const;

	/** Returns the horizonal FOV of the camera on this frame in degrees. */
	float GetHorizontalFieldOfView() const;

	/** Returns the vertical FOV of the camera on this frame in degrees. */
	float GetVerticalFieldOfView() const;

	/** Returns the effective horizontal field of view for the screen dimensions and fit mode in those dimensions */
	float GetHorizontalFieldOfViewForScreen( EAppleARKitBackgroundFitMode BackgroundFitMode ) const;
	
	/** Returns the effective horizontal field of view for the screen when a device is in portrait mode */
	float GetVerticalFieldOfViewForScreen( EAppleARKitBackgroundFitMode BackgroundFitMode ) const;

	/** Returns the effective horizontal field of view for the screen dimensions and fit mode in those dimensions */
	float GetHorizontalFieldOfViewForScreen( EAppleARKitBackgroundFitMode BackgroundFitMode, float ScreenWidth, float ScreenHeight ) const;
	
	/** Returns the effective vertical field of view for the screen dimensions and fit mode in those dimensions */
	float GetVerticalFieldOfViewForScreen( EAppleARKitBackgroundFitMode BackgroundFitMode, float ScreenWidth, float ScreenHeight ) const;

	/** For the given screen position, returns the normalised capture image coordinates accounting for the fit mode of the image on screen */
	FVector2D GetImageCoordinateForScreenPosition( FVector2D ScreenPosition, EAppleARKitBackgroundFitMode BackgroundFitMode ) const
	{
		// Use the global viewport size as the screen size
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize( ViewportSize );

		return GetImageCoordinateForScreenPosition( ScreenPosition, BackgroundFitMode, ViewportSize.X, ViewportSize.Y );
	}

	/** For the given screen position, returns the normalised capture image coordinates accounting for the fit mode of the image on screen */
	FVector2D GetImageCoordinateForScreenPosition( FVector2D ScreenPosition, EAppleARKitBackgroundFitMode BackgroundFitMode, float ScreenWidth, float ScreenHeight ) const;
};
