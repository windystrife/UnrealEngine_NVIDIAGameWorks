// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Factory for LandscapeGrassType assets
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "LandscapeGrassTypeFactory.generated.h"

UCLASS()
class ULandscapeGrassTypeFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	// End of UFactory interface
};
