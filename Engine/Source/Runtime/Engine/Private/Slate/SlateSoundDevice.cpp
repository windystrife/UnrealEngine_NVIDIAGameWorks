// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/SlateSoundDevice.h"
#include "Sound/SlateSound.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlateSoundDevice, Log, All);

void FSlateSoundDevice::PlaySound(const FSlateSound& Sound, int32 UserIndex) const
{
	if( GEngine && Sound.GetResourceObject() != nullptr )
	{
		FAudioDevice* const AudioDevice = GEngine->GetActiveAudioDevice();
		if(AudioDevice)
		{
			UObject* const Object = Sound.GetResourceObject();
			USoundBase* const SoundResource = Cast<USoundBase>(Object);
			if(SoundResource)
			{
				FActiveSound NewActiveSound;
				NewActiveSound.SetSound(SoundResource);
				NewActiveSound.bIsUISound = true;
				NewActiveSound.UserIndex = UserIndex;
				NewActiveSound.Priority = SoundResource->Priority;

				AudioDevice->AddNewActiveSound(NewActiveSound);
			}
			else if(Object)
			{
				// An Object but no SoundResource means that the FSlateSound is holding an invalid object; report that as an error
				UE_LOG(LogSlateSoundDevice, Error, TEXT("A sound contains a non-sound resource '%s'"), *Object->GetName());
			}
		}
	}
}

float FSlateSoundDevice::GetSoundDuration(const FSlateSound& Sound) const
{
	USoundBase* const SoundResource = Cast<USoundBase>(Sound.GetResourceObject());
	return (SoundResource) ? SoundResource->Duration : 0.0f;
}
