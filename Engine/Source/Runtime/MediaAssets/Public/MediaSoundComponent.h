// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SynthComponent.h"

#include "Containers/Array.h"
#include "HAL/CriticalSection.h"
#include "MediaSampleQueue.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

#include "MediaSoundComponent.generated.h"

class FMediaAudioResampler;
class FMediaPlayerFacade;
class IMediaAudioSample;
class IMediaPlayer;
class UMediaPlayer;


/**
 * Available media sound channel types.
 */
UENUM()
enum class EMediaSoundChannels
{
	/** Mono (1 channel). */
	Mono,

	/** Stereo (2 channels). */
	Stereo,

	/** Surround sound (7.1 channels; for UI). */
	Surround
};


/**
 * Implements a sound component for playing a media player's audio output.
 */
UCLASS(ClassGroup=Media, editinlinenew, meta=(BlueprintSpawnableComponent))
class MEDIAASSETS_API UMediaSoundComponent
	: public USynthComponent
{
	GENERATED_BODY()

public:

	/** Media sound channel type. */
	UPROPERTY(EditAnywhere, Category="UMediaSoundComponent")
	EMediaSoundChannels Channels;

	/** The media player asset associated with this component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Media")
	UMediaPlayer* MediaPlayer;

public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer Initialization parameters.
	 */
	UMediaSoundComponent(const FObjectInitializer& ObjectInitializer);

	/** Virtual destructor. */
	~UMediaSoundComponent();

public:

	void UpdatePlayer();

public:

	//~ UActorComponent interface

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

public:

	//~ USceneComponent interface

	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

protected:

	//~ USynthComponent interface

	virtual void Init(const int32 SampleRate) override;
	virtual void OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:

	/** Critical section for synchronizing access to PlayerFacadePtr. */
	FCriticalSection CriticalSection;

	/** The player facade that's currently providing texture samples. */
	TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> CurrentPlayerFacade;

	/** The audio resampler. */
	FMediaAudioResampler* Resampler;

	/** Audio sample queue. */
	TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> SampleQueue;
};
