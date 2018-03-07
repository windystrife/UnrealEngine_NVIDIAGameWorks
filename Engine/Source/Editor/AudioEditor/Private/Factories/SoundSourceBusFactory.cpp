// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundSourceBusFactory.h"
#include "Sound/SoundSourceBus.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Classes/Sound/AudioSettings.h"

USoundSourceBusFactory::USoundSourceBusFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundSourceBus::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundSourceBusFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USoundSourceBus* SoundSourceBus = NewObject<USoundSourceBus>(InParent, InName, Flags);

	FAudioDeviceManager* AudioDeviceManager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;
	if (AudioDeviceManager)
	{
		AudioDeviceManager->InitSoundSubmixes();
	}

	return SoundSourceBus;
}

bool USoundSourceBusFactory::CanCreateNew() const
{
	return GetDefault<UAudioSettings>()->IsAudioMixerEnabled();
}
