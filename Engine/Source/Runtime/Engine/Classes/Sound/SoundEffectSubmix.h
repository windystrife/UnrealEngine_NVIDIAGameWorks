// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundEffectPreset.h"
#include "Sound/SoundEffectBase.h"
#include "SoundEffectSubmix.generated.h"

class FSoundEffectSubmix;


/** This is here to make sure users don't mix up source and submix effects in the editor. Asset sorting, drag-n-drop, etc. */
UCLASS(config = Engine, hidecategories = Object, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectSubmixPreset : public USoundEffectPreset
{
	GENERATED_UCLASS_BODY()
};

/** Struct which has data needed to initialize the submix effect. */
struct FSoundEffectSubmixInitData
{
	void* PresetSettings;
	float SampleRate;
};

/** Struct which supplies audio data to submix effects on game thread. */
struct FSoundEffectSubmixInputData
{
	/** Ptr to preset data if new data is available. This will be nullptr if no new preset data has been set. */
	void* PresetData;
	
	/** The number of audio frames for this input data. 1 frame is an interleaved sample. */
	int32 NumFrames;

	/** The number of channels of this audio effect. */
	int32 NumChannels;

	/** The raw input audio buffer. Size is NumFrames * NumChannels */
	Audio::AlignedFloatBuffer* AudioBuffer;

	/** Sample accurate audio clock. */
	double AudioClock;
};

struct FSoundEffectSubmixOutputData
{
	/** The output audio buffer. */
	Audio::AlignedFloatBuffer* AudioBuffer;

	/** The number of channels in the output buffer. */
	int32 NumChannels;
};

class ENGINE_API FSoundEffectSubmix : public FSoundEffectBase
{
public:
	FSoundEffectSubmix() {}
	virtual ~FSoundEffectSubmix() {}

	/** Called on an audio effect at initialization on main thread before audio processing begins. */
	virtual void Init(const FSoundEffectSubmixInitData& InSampleRate) {};

	/** Called on game thread to allow submix effect to query game data if needed. */
	virtual void Tick() {}

	/** Override to down mix input audio to a desired channel count. */
	virtual uint32 GetDesiredInputChannelCountOverride() const
	{
		return INDEX_NONE;
	}

	/** Process the input block of audio. Called on audio thread. */
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) {};

	/** Processes audio in the submix effect. */
	void ProcessAudio(FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData);
};
