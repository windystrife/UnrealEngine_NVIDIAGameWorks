// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundConcurrencyFactory.h"
#include "Sound/SoundConcurrency.h"

USoundConcurrencyFactory::USoundConcurrencyFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundConcurrency::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundConcurrencyFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USoundConcurrency>(InParent, Name, Flags);
}

