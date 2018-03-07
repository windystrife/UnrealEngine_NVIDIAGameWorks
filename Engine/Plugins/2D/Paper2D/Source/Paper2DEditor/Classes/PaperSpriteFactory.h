// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Factory for sprites
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "PaperSpriteFactory.generated.h"

UCLASS()
class UPaperSpriteFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Initial texture to create the sprite from (Can be nullptr)
	class UTexture2D* InitialTexture;

	// Set bUseSourceRegion to get it to use/set initial sourceUV and dimensions
	bool bUseSourceRegion;
	FIntPoint InitialSourceUV;
	FIntPoint InitialSourceDimension;

	// UFactory interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
