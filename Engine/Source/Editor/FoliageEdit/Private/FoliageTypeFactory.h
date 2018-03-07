// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
* Factory for FoliageType assets
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "FoliageTypeFactory.generated.h"

UCLASS()
class UFoliageTypeFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const;
	// End of UFactory interface
};
