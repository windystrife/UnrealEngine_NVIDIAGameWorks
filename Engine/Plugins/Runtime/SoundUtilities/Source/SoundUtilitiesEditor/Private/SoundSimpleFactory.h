// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "SoundSimpleFactory.generated.h"

class USoundWave;

UCLASS(hidecategories = Object, MinimalAPI)
class USoundSimpleFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

	/** Sound waves to create the simple sound with */
	UPROPERTY()
	TArray<USoundWave*> SoundWaves;
};

