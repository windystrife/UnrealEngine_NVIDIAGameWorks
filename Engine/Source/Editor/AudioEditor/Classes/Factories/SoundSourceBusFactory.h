// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SoundSubmixFactory
//~=============================================================================

#pragma once
#include "Factories/Factory.h"
#include "SoundSourceBusFactory.generated.h"

UCLASS(MinimalAPI, hidecategories=Object)
class USoundSourceBusFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	//~ Begin UFactory Interface	
};
