// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerPlaceholderClass.h"
#include "UObject/Package.h"

//------------------------------------------------------------------------------
ULinkerPlaceholderClass::ULinkerPlaceholderClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
IMPLEMENT_CORE_INTRINSIC_CLASS(ULinkerPlaceholderClass, UClass, 
	{
	// @TODO: use the Class->Emit...() functions here to aid garbage 
	//        collection, so it has information on what class variables 
	//        hold onto UObject references
	}
);

//------------------------------------------------------------------------------
void ULinkerPlaceholderClass::BeginDestroy()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(IsMarkedResolved() || HasAnyFlags(RF_ClassDefaultObject));
	check(!HasKnownReferences());
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	// by this point, we really shouldn't have any properties left (they should 
	// have all got replaced), but just in case (so things don't blow up with a
	// missing class)...
	ResolveAllPlaceholderReferences(UObject::StaticClass());

	Super::BeginDestroy();
}

//------------------------------------------------------------------------------
void ULinkerPlaceholderClass::Bind()
{
	ClassConstructor = InternalConstructor<ULinkerPlaceholderClass>;
	ClassVTableHelperCtorCaller = InternalVTableHelperCtorCaller<ULinkerPlaceholderClass>;
	Super::Bind();

	ClassAddReferencedObjects = &ULinkerPlaceholderClass::AddReferencedObjects;
}


