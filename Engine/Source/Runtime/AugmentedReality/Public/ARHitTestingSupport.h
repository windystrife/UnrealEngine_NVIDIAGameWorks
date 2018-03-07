// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "ARHitTestingSupport.generated.h"

/**
 * A result of an intersection found during a hit-test.
 */
USTRUCT( BlueprintType, Category="AR", meta=(Experimental))
struct FARHitTestResult
{
	GENERATED_BODY();
	
	// Default constructor
	FARHitTestResult() {};
	
	/**
	 * The distance from the camera to the intersection in meters.
	 */
	UPROPERTY( BlueprintReadOnly, Category = "ARHitTestResult")
	float Distance;
	
	/**
	 * The transformation matrix that defines the intersection's rotation, translation and scale
	 * relative to the world.
	 */
	UPROPERTY( BlueprintReadOnly, Category = "ARHitTestResult")
	FTransform Transform;
};


class IARHitTestingSupport : public IModularFeature
{
public:
	//
	// MODULAR FEATURE SUPPORT
	//

	/**
	* Part of the pattern for supporting modular features.
	*
	* @return the name of the feature.
	*/
	static FName GetModularFeatureName()
	{
		static const FName ModularFeatureName = FName(TEXT("ARHitTesting"));
		return ModularFeatureName;
	}

	// @todo arkit : implement generic hit-test functionality
	//virtual bool ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults) = 0;
};
