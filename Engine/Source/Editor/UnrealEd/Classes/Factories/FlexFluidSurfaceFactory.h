// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// FlexFluidSurfaceFactory
//=============================================================================

#pragma once
#include "FlexFluidSurfaceFactory.generated.h"

UCLASS(hidecategories=Object)
class UFlexFluidSurfaceFactory : public UFactory
{
	GENERATED_UCLASS_BODY()


	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface	
};



