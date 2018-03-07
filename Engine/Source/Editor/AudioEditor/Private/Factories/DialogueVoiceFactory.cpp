// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/DialogueVoiceFactory.h"
#include "Sound/DialogueVoice.h"

UDialogueVoiceFactory::UDialogueVoiceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UDialogueVoice::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDialogueVoiceFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UObject>(InParent, Class, Name, Flags);
}
