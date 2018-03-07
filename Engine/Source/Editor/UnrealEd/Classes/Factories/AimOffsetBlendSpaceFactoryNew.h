// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// UAimOffsetBlendSpaceFactoryNew
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/BlendSpaceFactoryNew.h"
#include "AimOffsetBlendSpaceFactoryNew.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UAimOffsetBlendSpaceFactoryNew : public UBlendSpaceFactoryNew
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface
};



