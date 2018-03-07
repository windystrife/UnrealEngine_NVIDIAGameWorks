// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ARHitTestingSupport.h"
#include "ARTrackingQuality.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARBlueprintFunctionLibrary.generated.h"


UCLASS()
class AUGMENTEDREALITY_API UARBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//UFUNCTION(BlueprintCallable, Category = "AugmentedReality|LineTrace", meta = (WorldContext = "WorldContextObject", Keywords = "ar augmentedreality augmented reality line trace hit test"))
	//static bool ARLineTraceFromScreenPoint(UObject* WorldContextObject, const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults);
	
	UFUNCTION(BlueprintCallable, Category = "AugmentedReality|Tracking", meta = (WorldContext = "WorldContextObject", Keywords = "ar augmentedreality augmented reality tracking quality"))
	static EARTrackingQuality GetTrackingQuality( UObject* WorldContextObject );
	
};
