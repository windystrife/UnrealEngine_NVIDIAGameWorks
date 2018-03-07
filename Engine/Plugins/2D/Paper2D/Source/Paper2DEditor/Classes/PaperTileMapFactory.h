// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Factory for tile maps
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "PaperTileMapFactory.generated.h"

UCLASS()
class UPaperTileMapFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Initial tile set to create the tile map from (Can be nullptr)
	class UPaperTileSet* InitialTileSet;

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
