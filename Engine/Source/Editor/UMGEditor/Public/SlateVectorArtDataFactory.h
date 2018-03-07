// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "SlateVectorArtDataFactory.generated.h"

UCLASS(hidecategories = Object, collapsecategories, MinimalAPI)
class USlateVectorArtDataFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// ~ UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// ~ UFactory Interface	
};
