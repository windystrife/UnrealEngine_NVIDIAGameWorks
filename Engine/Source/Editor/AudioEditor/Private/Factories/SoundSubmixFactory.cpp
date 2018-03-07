// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundSubmixFactory.h"
#include "Sound/SoundSubmix.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Classes/Sound/AudioSettings.h"

USoundSubmixFactory::USoundSubmixFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundSubmix::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundSubmixFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USoundSubmix* SoundSubmix = NewObject<USoundSubmix>(InParent, InName, Flags);

	class FAudioDeviceManager* AudioDeviceManager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;
	if (AudioDeviceManager)
	{
		AudioDeviceManager->InitSoundSubmixes();
	}

	return SoundSubmix;
}

bool USoundSubmixFactory::CanCreateNew() const
{
	return GetDefault<UAudioSettings>()->IsAudioMixerEnabled();
}
