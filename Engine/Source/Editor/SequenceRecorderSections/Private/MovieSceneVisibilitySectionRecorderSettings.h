// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MovieSceneVisibilitySectionRecorderSettings.generated.h"

UCLASS()
class UMovieSceneVisibilitySectionRecorderSettings : public UObject
{
public:
	GENERATED_BODY()

	UMovieSceneVisibilitySectionRecorderSettings()
		: bRecordVisibility(true)
	{}

	/** Whether to record actor visibility. */
	UPROPERTY(EditAnywhere, Category = "Actor Recording")
	bool bRecordVisibility;
};

