// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ARKit
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
#import <ARKit/ARKit.h>
#endif // ARKIT_SUPPORT

// AppleARKit
#include "AppleARKitAnchor.h"
#include "AppleARKitPlaneAnchor.generated.h"

UCLASS( BlueprintType )
class APPLEARKIT_API UAppleARKitPlaneAnchor : public UAppleARKitAnchor
{
	GENERATED_BODY()

public: 

	/**
	 * The center of the plane in the anchor’s coordinate space.
	 */
	UFUNCTION( BlueprintPure, Category = "AppleARKitPlaneAnchor" )
	FVector GetCenter() const;

	/**
	 * The extent of the plane in the anchor’s coordinate space.
	 */
	UFUNCTION( BlueprintPure, Category = "AppleARKitPlaneAnchor")
	FVector GetExtent() const;

	UFUNCTION( BlueprintPure, Category = "AppleARKitPlaneAnchor")
	FTransform GetTransformToCenter() const;

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

	virtual void Update_DelegateThread( ARAnchor* Anchor ) override;

#endif // ARKIT_SUPPORT

private:

	/**
	 * The center of the plane in the anchor’s coordinate space.
	 */
	FVector Center;

	/**
	 * The extent of the plane in the anchor’s coordinate space.
	 */
	FVector Extent;
};
