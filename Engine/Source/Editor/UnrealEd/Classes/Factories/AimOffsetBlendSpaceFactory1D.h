// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// UAimOffsetBlendSpaceFactory1D
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/BlendSpaceFactory1D.h"
#include "AimOffsetBlendSpaceFactory1D.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UAimOffsetBlendSpaceFactory1D : public UBlendSpaceFactory1D
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface
};



