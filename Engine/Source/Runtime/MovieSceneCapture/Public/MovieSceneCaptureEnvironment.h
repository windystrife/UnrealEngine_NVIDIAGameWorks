// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "MovieSceneCaptureEnvironment.generated.h"

UCLASS()
class UMovieSceneCaptureEnvironment : public UObject
{
public:
	GENERATED_BODY()

	/** Get the frame number of the current capture */
	UFUNCTION(BlueprintPure, Category="Cinematics|Capture")
	static int32 GetCaptureFrameNumber();

	/** Get the total elapsed time of the current capture in seconds */
	UFUNCTION(BlueprintPure, Category="Cinematics|Capture")
	static float GetCaptureElapsedTime();
};
