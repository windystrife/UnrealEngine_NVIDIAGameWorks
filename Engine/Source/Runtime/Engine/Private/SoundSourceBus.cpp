// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundSourceBus.h"
#include "AudioDeviceManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "ActiveSound.h"

void USoundSourceBus::PostLoad()
{
	Super::PostLoad();

	// This is a bus. This will result in the decompression type to be set as DTYPE_Bus. Audio won't be generated from this object but from instance data in audio mixer.
	bIsBus = true;

	// Allow users to manually set the source bus duration
	Duration = GetDuration();

	// This sound wave is looping if the source bus duration is 0.0f
	bLooping = (SourceBusDuration == 0.0f);

	// Keep playing this bus when the volume is 0
	// Note source buses can't ever be truly virtual as they are procedurally generated.
	bVirtualizeWhenSilent = !bAutoDeactivateWhenSilent;

	// Use the main/default audio device for storing and retrieving sound class properties
	FAudioDeviceManager* AudioDeviceManager = (GEngine ? GEngine->GetAudioDeviceManager() : nullptr);

	// Set the channels equal to the users channel count choice
	switch (SourceBusChannels)
	{
		case ESourceBusChannels::Mono:
			NumChannels = 1;
			break;

		case ESourceBusChannels::Stereo:
			NumChannels = 2;
			break;
	}
}

void USoundSourceBus::BeginDestroy()
{
	Super::BeginDestroy();
}

#if WITH_EDITOR
void USoundSourceBus::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// stub
}
#endif

bool USoundSourceBus::IsPlayable() const
{
	return true;
}

float USoundSourceBus::GetDuration()
{
	return (SourceBusDuration > 0.0f) ? SourceBusDuration : INDEFINITELY_LOOPING_DURATION;
}

