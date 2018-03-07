// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/DialogueWaveFactory.h"
#include "Sound/DialogueWave.h"
#include "Sound/SoundWave.h"

UDialogueWaveFactory::UDialogueWaveFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UDialogueWave::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDialogueWaveFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class == SupportedClass);
	UDialogueWave* DialogueWave = NewObject<UDialogueWave>(InParent, Name, Flags);

	if (InitialSoundWave)
	{
		DialogueWave->SpokenText = InitialSoundWave->SpokenText;
		DialogueWave->bMature = InitialSoundWave->bMature;
	}

	DialogueWave->UpdateContext(DialogueWave->ContextMappings[0], InitialSoundWave, InitialSpeakerVoice, InitialTargetVoices);

	return DialogueWave;
}
