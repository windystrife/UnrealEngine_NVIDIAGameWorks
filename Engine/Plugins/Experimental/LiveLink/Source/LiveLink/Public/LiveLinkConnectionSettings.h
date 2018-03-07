// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "LiveLinkConnectionSettings.generated.h"

USTRUCT()
struct FLiveLinkConnectionSettings
{
	GENERATED_USTRUCT_BODY()

	FLiveLinkConnectionSettings() : bUseInterpolation(false), InterpolationOffset(0.5f) {}
	
	// Should this connection use interpolation
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bUseInterpolation;
	
	// When interpolating: how far back from current time should we read the buffer (in seconds)
	UPROPERTY(EditAnywhere, Category = Settings)
	float InterpolationOffset;
};