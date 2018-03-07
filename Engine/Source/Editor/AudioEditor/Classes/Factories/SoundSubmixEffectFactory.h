// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSubmix.h"
#include "Factories/Factory.h"
#include "SoundSubmixEffectFactory.generated.h"

UCLASS(MinimalAPI, hidecategories=Object)
class USoundSubmixEffectFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	/** The type of sound source effect preset that will be created */
	UPROPERTY(EditAnywhere, Category = CurveFactory)
	TSubclassOf<USoundEffectSubmixPreset> SoundEffectSubmixPresetClass;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	//~ Begin UFactory Interface	
};


