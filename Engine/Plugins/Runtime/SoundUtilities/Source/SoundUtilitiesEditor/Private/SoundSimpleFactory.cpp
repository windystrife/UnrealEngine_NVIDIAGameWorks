// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundSimpleFactory.h"
#include "SoundSimple.h"

USoundSimpleFactory::USoundSimpleFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundSimple::StaticClass();

	bCreateNew = false;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundSimpleFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USoundSimple* SoundSimple = NewObject<USoundSimple>(InParent, Name, Flags);

	float Duration = 0.0f;

	// Add the sound waves with default variation entries
	for (int32 i = 0; i < SoundWaves.Num(); ++i)
	{
		FSoundVariation NewVariation;
		NewVariation.SoundWave = SoundWaves[i];

		if (SoundWaves[i]->Duration > Duration)
		{
			Duration = SoundWaves[i]->Duration;
		}

		SoundSimple->Variations.Add(NewVariation);
	}

	// Write out the duration to be the longest duration
	SoundSimple->Duration = Duration;

	return SoundSimple;
}
