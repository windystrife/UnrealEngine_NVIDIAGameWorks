// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "ImagePlateFileSequenceFactory.generated.h"

UCLASS(MinimalAPI)
class UImagePlateFileSequenceFactory : public UFactory
{
	GENERATED_BODY()

	UImagePlateFileSequenceFactory(const FObjectInitializer& ObjectInitializer);

	virtual FText GetDisplayName() const override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
};
