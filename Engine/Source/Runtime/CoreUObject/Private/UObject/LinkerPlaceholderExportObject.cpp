// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerPlaceholderExportObject.h"

//------------------------------------------------------------------------------
ULinkerPlaceholderExportObject::ULinkerPlaceholderExportObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
IMPLEMENT_CORE_INTRINSIC_CLASS(ULinkerPlaceholderExportObject, UObject,
	{
	}
);

//------------------------------------------------------------------------------
void ULinkerPlaceholderExportObject::BeginDestroy()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(IsMarkedResolved() || HasAnyFlags(RF_ClassDefaultObject));
	check(!HasKnownReferences());
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	Super::BeginDestroy();
}
