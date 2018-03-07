// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SoundSurroundFactory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "Audio.h"
#include "SoundSurroundFactory.generated.h"

const FString SurroundSpeakerLocations[SPEAKER_Count] =
{
	TEXT("_fl"),		// SPEAKER_FrontLeft
	TEXT("_fr"),		// SPEAKER_FrontRight
	TEXT("_fc"),		// SPEAKER_FrontCenter
	TEXT("_lf"),		// SPEAKER_LowFrequency
	TEXT("_sl"),		// SPEAKER_SideLeft
	TEXT("_sr"),		// SPEAKER_SideRight
	TEXT("_bl"),		// SPEAKER_BackLeft
	TEXT("_br")			// SPEAKER_BackRight
};

UCLASS(MinimalAPI, hidecategories=Object)
class USoundSurroundFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	float CueVolume;


	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn ) override;
	virtual bool FactoryCanImport( const FString& Filename ) override;
	//~ End UFactory Interface
};
