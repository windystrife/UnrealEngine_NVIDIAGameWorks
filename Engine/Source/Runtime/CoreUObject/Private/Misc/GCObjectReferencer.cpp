// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GCObjectReferencer.cpp: Implementation of UGCObjectReferencer
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/Casts.h"
#include "UObject/GCObject.h"

// Global GC state flags
extern bool GObjIncrementalPurgeIsInProgress;
extern bool GObjUnhashUnreachableIsInProgress;

void UGCObjectReferencer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UGCObjectReferencer* This = CastChecked<UGCObjectReferencer>(InThis);
	// Note we're not locking ReferencedObjectsCritical here because we guard
	// against adding new references during GC in AddObject and RemoveObject.
	// Let each registered object handle its AddReferencedObjects call
	for (FGCObject* Object : This->ReferencedObjects)
	{
		check(Object);
		Object->AddReferencedObjects(Collector);
	}
	Super::AddReferencedObjects( This, Collector );
}

void UGCObjectReferencer::AddObject(FGCObject* Object)
{
	check(Object);
	check(GObjUnhashUnreachableIsInProgress || GObjIncrementalPurgeIsInProgress || !IsGarbageCollecting());	FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
	// Make sure there are no duplicates. Should be impossible...
	ReferencedObjects.AddUnique(Object);
}

void UGCObjectReferencer::RemoveObject(FGCObject* Object)
{
	check(Object);
	check(GObjUnhashUnreachableIsInProgress || GObjIncrementalPurgeIsInProgress || !IsGarbageCollecting());	FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
	ReferencedObjects.Remove(Object);
}

void UGCObjectReferencer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Make sure FGCObjects that are around after exit purge don't
		// reference this object.
		check( FGCObject::GGCObjectReferencer == this );
		FGCObject::GGCObjectReferencer = NULL;
	}

	Super::FinishDestroy();
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UGCObjectReferencer, UObject, 
	{
		Class->ClassAddReferencedObjects = &UGCObjectReferencer::AddReferencedObjects;
	}
);

/** Static used for calling AddReferencedObjects on non-UObject objects */
UGCObjectReferencer* FGCObject::GGCObjectReferencer = NULL;


