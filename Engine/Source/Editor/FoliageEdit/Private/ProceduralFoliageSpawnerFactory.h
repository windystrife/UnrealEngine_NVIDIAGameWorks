// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Factory for ProceduralFoliageSpawner assets
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "ProceduralFoliageSpawnerFactory.generated.h"

UCLASS()
class UProceduralFoliageSpawnerFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual bool ShouldShowInNewMenu() const override;
	// End of UFactory interface
};
