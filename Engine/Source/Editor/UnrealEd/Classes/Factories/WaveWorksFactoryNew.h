// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// WaveWorksFactoryNew
//=============================================================================

#pragma once
#include "WaveWorksFactoryNew.generated.h"

UCLASS(hidecategories=Object)
class UWaveWorksFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()


	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface
};
