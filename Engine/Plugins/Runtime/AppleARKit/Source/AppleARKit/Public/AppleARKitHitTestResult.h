// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ARKit
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
#import <ARKit/ARKit.h>
#endif // ARKIT_SUPPORT

// AppleARKit
#include "AppleARKitHitTestResult.generated.h"

/**
 * Option set of hit-test result types.
 */
UENUM( BlueprintType, Category="AppleARKit", meta=(Bitflags) )
enum class EAppleARKitHitTestResultType : uint8
{
	None = 0,

    /** Result type from intersecting the nearest feature point. */
    FeaturePoint = 1 UMETA(DisplayName = "Feature Point"),
    
    /** Result type from intersecting a horizontal plane estimate, determined for the current frame. */
    EstimatedHorizontalPlane = 2 UMETA(DisplayName = "Estimated Horizontal Plane"),
    
    /** Result type from intersecting with an existing plane anchor. */
    ExistingPlane = 4 UMETA(DisplayName = "Existing Plane"),
    
    /** Result type from intersecting with an existing plane anchor. */
    ExistingPlaneUsingExtent = 8 UMETA(DisplayName= "Existing Plane Using Extent")
};
ENUM_CLASS_FLAGS(EAppleARKitHitTestResultType);

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

/** Conversion function from ARKit native ARHitTestResultType */
EAppleARKitHitTestResultType ToEAppleARKitHitTestResultType(ARHitTestResultType InTypes);

/** Conversion function to ARKit native ARHitTestResultType */
ARHitTestResultType ToARHitTestResultType(EAppleARKitHitTestResultType InTypes);

#endif

/**
 * A result of an intersection found during a hit-test.
 */
USTRUCT( BlueprintType, Category="AppleARKit")
struct APPLEARKIT_API FAppleARKitHitTestResult
{
    GENERATED_BODY();

    // Default constructor
	FAppleARKitHitTestResult() {};

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

	/** 
	 * This is a conversion copy-constructor that takes a raw ARHitTestResult and fills this 
	 * structs members with the UE4-ified versions of ARHitTestResult's properties.
	 */ 
	FAppleARKitHitTestResult( ARHitTestResult* InARHitTestResult, class UAppleARKitAnchor* InAnchor = nullptr, float WorldToMetersScale = 100.0f );

#endif // #ARKIT_SUPPORT

	/**
	 * The type of the hit-test result.
	 */
    UPROPERTY( BlueprintReadOnly, Category = "AppleARKitHitTestResult" )
    EAppleARKitHitTestResultType Type;
    
	/**
	 * The distance from the camera to the intersection in meters.
	 */
    UPROPERTY( BlueprintReadOnly, Category = "AppleARKitHitTestResult")
	float Distance;

	/**
	 * The transformation matrix that defines the intersection's rotation, translation and scale
	 * relative to the world.
	 */
    UPROPERTY( BlueprintReadOnly, Category = "AppleARKitHitTestResult")
	FTransform Transform;

	/**
	 * The anchor that the hit-test intersected.
	 * 
	 * An anchor will only be provided for existing plane result types.
	 */
	UPROPERTY( BlueprintReadOnly, Category = "AppleARKitHitTestResult")
	class UAppleARKitAnchor* Anchor = nullptr;
};
