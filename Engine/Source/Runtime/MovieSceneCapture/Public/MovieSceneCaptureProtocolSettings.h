// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MovieSceneCaptureProtocolSettings.generated.h"

struct FMovieSceneCaptureSettings;

UCLASS()
class MOVIESCENECAPTURE_API UMovieSceneCaptureProtocolSettings : public UObject
{
public:

	GENERATED_BODY()

	/**
	 * Called when this protocol has been released
	 */
	virtual void OnReleaseConfig(FMovieSceneCaptureSettings& InSettings) {}

	/**
	 * Called when this protocol has been loaded
	 */
	virtual void OnLoadConfig(FMovieSceneCaptureSettings& InSettings) {}
};

