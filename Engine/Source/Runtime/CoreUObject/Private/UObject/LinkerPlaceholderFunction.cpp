// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerPlaceholderFunction.h"

//------------------------------------------------------------------------------
ULinkerPlaceholderFunction::ULinkerPlaceholderFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
IMPLEMENT_CORE_INTRINSIC_CLASS(ULinkerPlaceholderFunction, UFunction,
	{
	}
);
