// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/LinkerPlaceholderBase.h"

/**  
 * A utility class for the deferred dependency loader, used to stub in temporary
 * export objects so we don't spawn any Blueprint class instances before the 
 * class is fully regenerated.
 */ 
class ULinkerPlaceholderExportObject : public UObject, public FLinkerPlaceholderBase
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(ULinkerPlaceholderExportObject, UObject, /*TStaticFlags =*/0, TEXT("/Script/CoreUObject"), /*TStaticCastFlags =*/0, NO_API)

	ULinkerPlaceholderExportObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// UObject interface
	virtual void BeginDestroy() override;
	// End of UObject interface

	// FLinkerPlaceholderBase interface
	virtual UObject* GetPlaceholderAsUObject() override { return (UObject*)(this); }
	// End of FLinkerPlaceholderBase interface
}; 
