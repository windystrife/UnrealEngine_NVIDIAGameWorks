// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "ARTrackingQuality.generated.h"

UENUM(BlueprintType, Category="AR", meta=(Experimental))
enum class EARTrackingQuality : uint8
{
    /** The tracking quality is not available. */
    NotAvailable,
    
    /** The tracking quality is limited, relying only on the device's motion. */
    Limited,
	
    /** The tracking quality is good. */
    Normal
};

class IARTrackingQuality : public IModularFeature
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
		static const FName ModularFeatureName = FName(TEXT("TrackingQuality"));
		return ModularFeatureName;
	}
	
	virtual EARTrackingQuality ARGetTrackingQuality() const = 0;
};
