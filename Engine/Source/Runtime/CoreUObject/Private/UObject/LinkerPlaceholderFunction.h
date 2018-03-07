// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/LinkerPlaceholderBase.h"

/**  */
class ULinkerPlaceholderFunction : public UFunction, public TLinkerImportPlaceholder<UFunction>
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(ULinkerPlaceholderFunction, UFunction, /*TStaticFlags =*/0, TEXT("/Script/CoreUObject"), /*TStaticCastFlags =*/0, NO_API)

	ULinkerPlaceholderFunction(const FObjectInitializer& ObjectInitializer);

	// FLinkerPlaceholderBase interface 
	virtual UObject* GetPlaceholderAsUObject() override { return (UObject*)(this); }
	// End of FLinkerPlaceholderBase interface
};
