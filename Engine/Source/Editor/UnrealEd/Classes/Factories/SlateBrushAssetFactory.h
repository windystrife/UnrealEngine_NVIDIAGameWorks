// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "SlateBrushAssetFactory.generated.h"

/** Factory for creating SlateBrushAssets */
UCLASS(hidecategories=Object, MinimalAPI)
class USlateBrushAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	/** An initial texture to assign to the newly created slate brush */
	UPROPERTY()
	class UTexture2D* InitialTexture;

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface
};



