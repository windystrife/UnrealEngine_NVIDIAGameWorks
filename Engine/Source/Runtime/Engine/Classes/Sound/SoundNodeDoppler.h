// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AudioDevice.h"
#include "Sound/SoundNode.h"
#include "SoundNodeDoppler.generated.h"

struct FActiveSound;
struct FSoundParseParameters;

/** 
 * Computes doppler pitch shift
 */
UCLASS(hidecategories=Object, editinlinenew, meta=( DisplayName="Doppler" ))
class USoundNodeDoppler : public USoundNode
{
	GENERATED_UCLASS_BODY()

	/* How much to scale the doppler shift (1.0 is normal). */
	UPROPERTY(EditAnywhere, Category=Doppler )
	float DopplerIntensity;


public:
	//~ Begin USoundNode Interface. 
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	//~ End USoundNode Interface. 

protected:
	// @todo document
	float GetDopplerPitchMultiplier(FListener const& InListener, const FVector Location, const FVector Velocity) const;
};



