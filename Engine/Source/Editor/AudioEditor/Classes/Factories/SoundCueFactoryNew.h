// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SoundCueFactoryNew
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "SoundCueFactoryNew.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class USoundCueFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()


	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

	/** An initial sound wave to place in the newly created cue */
	UPROPERTY()
	class USoundWave* InitialSoundWave;

	/** An initial dialogue wave to place in the newly created cue */
	UPROPERTY()
	class UDialogueWave* InitialDialogueWave;
};



