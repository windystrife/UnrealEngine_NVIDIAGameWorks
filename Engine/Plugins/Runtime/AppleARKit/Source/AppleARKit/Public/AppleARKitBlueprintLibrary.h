// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AppleARKitFrame.h"
#include "AppleARKitHitTestResult.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AppleARKitBlueprintLibrary.generated.h"

UCLASS()
class APPLEARKIT_API UAppleARKitBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	/**
	 * Returns the last processed FAppleARKitFrame from the session.
	 *
	 * When called from the game thread, we ensure a single frame is returned for the duration
	 * of the game thread update by watching for changes to GFrameNumber to trigger pulling a
	 * new frame from the session.
	 */
	UFUNCTION( BlueprintCallable, Category="AppleARKit", meta = (WorldContext = "WorldContextObject", Keywords = "ar augmentedreality augmented reality frame"))
	static bool GetCurrentFrame( UObject* WorldContextObject, FAppleARKitFrame& OutCurrentFrame );
	
	/**
	 * Searches the last processed frame for anchors corresponding to a point in the captured image.
	 *
	 * A 2D point in the captured image's coordinate space can refer to any point along a line segment
	 * in the 3D coordinate space. Hit-testing is the process of finding anchors of a frame located along this line segment.
	 *
	 * NOTE: The hit test locations are reported in ARKit space. For hit test results
	 * in game world coordinates, you're after UAppleARKitCameraComponent::HitTestAtScreenPosition
	 *
	 * @param ScreenPosition The viewport pixel coordinate of the trace origin.
	 */
	UFUNCTION( BlueprintCallable, Category="AppleARKit", meta = (WorldContext = "WorldContextObject", Keywords = "ar augmentedreality augmented reality trace hittest hit line"))
	static bool HitTestAtScreenPosition_TrackingSpace( UObject* WorldContextObject, const FVector2D ScreenPosition, EAppleARKitHitTestResultType Types, TArray< FAppleARKitHitTestResult >& OutResults );
};
