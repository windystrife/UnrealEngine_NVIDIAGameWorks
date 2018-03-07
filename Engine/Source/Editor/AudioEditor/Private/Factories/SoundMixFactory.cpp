// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundMixFactory.h"
#include "Sound/SoundMix.h"

USoundMixFactory::USoundMixFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = USoundMix::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundMixFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USoundMix* Mix = NewObject<USoundMix>(InParent, InName, Flags);

	return Mix;
}
