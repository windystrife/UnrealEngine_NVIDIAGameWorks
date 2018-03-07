// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Factory for tile sets
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "PaperTileSetFactory.generated.h"

UCLASS()
class UPaperTileSetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Initial texture to create the tile set from (Can be nullptr)
	class UTexture2D* InitialTexture;

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
