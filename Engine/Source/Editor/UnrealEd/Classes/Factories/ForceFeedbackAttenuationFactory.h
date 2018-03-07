// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// ForceFeedbackAttenuationFactory
//~=============================================================================

#pragma once
#include "Factories/Factory.h"
#include "ForceFeedbackAttenuationFactory.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UForceFeedbackAttenuationFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};



