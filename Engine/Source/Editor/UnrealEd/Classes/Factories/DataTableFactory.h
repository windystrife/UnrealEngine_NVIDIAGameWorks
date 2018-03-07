// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "DataTableFactory.generated.h"

UCLASS(hidecategories=Object)
class UNREALED_API UDataTableFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UScriptStruct* Struct;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface
};



