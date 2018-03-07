// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/LinkerPlaceholderBase.h"

/**  
 * A utility class for the deferred dependency loader, used to stub in temporary
 * class references so we don't have to load blueprint resources for their class.
 * Holds on to references where this is currently being utilized, so we can 
 * easily replace references to it later (once the real class is available).
 */ 
class ULinkerPlaceholderClass : public UClass, public TLinkerImportPlaceholder<UClass>
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(ULinkerPlaceholderClass, UClass, /*TStaticFlags =*/0, TEXT("/Script/CoreUObject"), /*TStaticCastFlags =*/0, NO_API)

	ULinkerPlaceholderClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// UObject interface.
	virtual void BeginDestroy() override;
	// End of UObject interface.

	// UField interface.
 	virtual void Bind() override;
	// End of UField interface.

	// FLinkerPlaceholderBase interface 
	virtual UObject* GetPlaceholderAsUObject() override { return (UObject*)(this); }
	// End of FLinkerPlaceholderBase interface
}; 

