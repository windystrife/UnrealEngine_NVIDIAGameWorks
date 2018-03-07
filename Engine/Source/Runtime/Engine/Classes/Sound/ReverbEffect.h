// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ReverbEffect.generated.h"

UCLASS(BlueprintType, hidecategories=object, MinimalAPI)
class UReverbEffect : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Density - 0.0 < 1.0 < 1.0 - Coloration of the late reverb - lower value is more grainy */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "1.0"), EditAnywhere)
	float		Density;

	/** Diffusion - 0.0 < 1.0 < 1.0 - Echo density in the reverberation decay - lower is more grainy */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "1.0"), EditAnywhere)
	float		Diffusion;

	/** Reverb Gain - 0.0 < 0.32 < 1.0 - overall reverb gain - master volume control */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "1.0"), EditAnywhere)
	float		Gain;

	/** Reverb Gain High Frequency - 0.0 < 0.89 < 1.0 - attenuates the high frequency reflected sound */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "1.0"), EditAnywhere)
	float		GainHF;

	/** Decay Time - 0.1 < 1.49 < 20.0 Seconds - larger is more reverb */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.1", ClampMax = "20.0"), EditAnywhere)
	float		DecayTime;

	/** Decay High Frequency Ratio - 0.1 < 0.83 < 2.0 - how much the quicker or slower the high frequencies decay relative to the lower frequencies. */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.1", ClampMax = "2.0"), EditAnywhere)
	float		DecayHFRatio;

	/** Reflections Gain - 0.0 < 0.05 < 3.16 - controls the amount of initial reflections */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "3.16"), EditAnywhere)
	float		ReflectionsGain;

	/** Reflections Delay - 0.0 < 0.007 < 0.3 Seconds - the time between the listener receiving the direct path sound and the first reflection */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "0.3"), EditAnywhere)
	float		ReflectionsDelay;

	/** Late Reverb Gain - 0.0 < 1.26 < 10.0 - gain of the late reverb */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "10.0"), EditAnywhere)
	float		LateGain;

	/** Late Reverb Delay - 0.0 < 0.011 < 0.1 Seconds - time difference between late reverb and first reflections */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "0.1"), EditAnywhere)
	float		LateDelay;

	/** Air Absorption - 0.0 < 0.994 < 1.0 - lower value means more absorption */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "1.0"), EditAnywhere)
	float		AirAbsorptionGainHF;

	/** Room Rolloff - 0.0 < 0.0 < 10.0 - multiplies the attenuation due to distance */
	UPROPERTY(Category=ReverbParameters, meta=(ClampMin = "0.0", ClampMax = "10.0"), EditAnywhere)
	float		RoomRolloffFactor;

#if WITH_EDITORONLY_DATA
	/** Transient property used to trigger real-time updates of the reverb for real-time editor previewing */
	UPROPERTY(transient)
	uint32 bChanged : 1;
#endif

protected:

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
