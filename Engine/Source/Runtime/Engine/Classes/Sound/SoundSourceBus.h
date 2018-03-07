// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundSourceBusSend.h"
#include "SoundSourceBus.generated.h"

class USoundSourceBus;
class USoundWaveProcedural;
class FAudioDevice;

// The number of channels to mix audio into the source bus
UENUM(BlueprintType)
enum class ESourceBusChannels : uint8
{
	Mono,
	Stereo,
};

// A source bus is a type of USoundBase and can be "played" like any sound.
UCLASS(hidecategories= (Compression, SoundWave, Streaming, Subtitles, Sound, Info, ImportSettings), ClassGroup = Sound, meta = (BlueprintSpawnableComponent))
class ENGINE_API USoundSourceBus : public USoundWave
{
	GENERATED_BODY()

public:

	/** How many channels to use for the source bus. */
	UPROPERTY(EditAnywhere, Category = BusProperties)
	ESourceBusChannels SourceBusChannels;

	/** The duration (in seconds) to use for the source bus. A duration of 0.0 indicates to play the source bus indefinitely. */
	UPROPERTY(EditAnywhere, Category = BusProperties, meta = (UIMin = 0.0, ClampMin = 0.0))
	float SourceBusDuration;

	/** Stop the source bus when the volume goes to zero. */
	UPROPERTY(EditAnywhere, Category = BusProperties, meta = (UIMin = 0.0, ClampMin = 0.0))
	uint32 bAutoDeactivateWhenSilent:1;

	//~ Begin UObject Interface. 
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface. 

	//~ Begin USoundBase Interface.
	virtual bool IsPlayable() const override;
	virtual float GetDuration() override;
	//~ End USoundBase Interface.

protected:

	uint32 DurationSamples;
	bool bInitialized;
};
