// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "LevelSequenceFactoryNew.generated.h"

/**
 * Implements a factory for ULevelSequence objects.
 */
UCLASS(hidecategories=Object)
class ULevelSequenceFactoryNew
	: public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	// UFactory Interface

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
};
