// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WeakObjectPtr.cpp: Weak pointer to UObject
=============================================================================*/

#include "UObject/WeakObjectPtr.h"
#include "UObject/Object.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeakObjectPtr, Log, All);

/*-----------------------------------------------------------------------------------------------------------
	Base serial number management.
-------------------------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------------------
	FWeakObjectPtr
-------------------------------------------------------------------------------------------------------------*/

/**  
 * Copy from an object pointer
 * @param Object object to create a weak pointer to
**/
void FWeakObjectPtr::operator=(const class UObject *Object)
{
	if (Object // && UObjectInitialized() we might need this at some point, but it is a speed hit we would prefer to avoid
		)
	{
		ObjectIndex = GUObjectArray.ObjectToIndex((UObjectBase*)Object);
		ObjectSerialNumber = GUObjectArray.AllocateSerialNumber(ObjectIndex);
		checkSlow(SerialNumbersMatch());
	}
	else
	{
		Reset();
	}
}

bool FWeakObjectPtr::IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest) const
{
	// This is the external function, so we just pass through to the internal inlined method.
	return Internal_IsValid(bEvenIfPendingKill, bThreadsafeTest);
}

bool FWeakObjectPtr::IsValid() const
{
	// Using literals here allows the optimizer to remove branches later down the chain.
	return Internal_IsValid(false, false);
}

bool FWeakObjectPtr::IsStale(bool bEvenIfPendingKill, bool bThreadsafeTest) const
{
	if (ObjectSerialNumber == 0)
	{
		checkSlow(ObjectIndex == 0 || ObjectIndex == -1); // otherwise this is a corrupted weak pointer
		return false;
	}
	if (ObjectIndex < 0)
	{
		return true;
	}
	FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(ObjectIndex);
	if (!ObjectItem)
	{
		return true;
	}
	if (!SerialNumbersMatch(ObjectItem))
	{
		return true;
	}
	if (bThreadsafeTest)
	{
		return false;
	}
	return GUObjectArray.IsStale(ObjectItem, bEvenIfPendingKill);
}

UObject* FWeakObjectPtr::Get(/*bool bEvenIfPendingKill = false*/) const
{
	// Using a literal here allows the optimizer to remove branches later down the chain.
	return Internal_Get(false);
}

UObject* FWeakObjectPtr::Get(bool bEvenIfPendingKill) const
{
	return Internal_Get(bEvenIfPendingKill);
}

UObject* FWeakObjectPtr::GetEvenIfUnreachable() const
{
	UObject* Result = nullptr;
	if (Internal_IsValid(true, true))
	{
		FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(GetObjectIndex(), true);
		Result = static_cast<UObject*>(ObjectItem->Object);
	}
	return Result;
}

void FWeakObjectPtr::Serialize(FArchive& Ar)
{
	// NOTE: When changing this function, make sure to update the SavePackage.cpp version in the import and export tagger.

	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want weak object pointers to keep objects from being garbage
	// collected.  That would defeat the whole purpose of a weak object pointer!
	// However, when modifying both kinds of references we want to serialize and writeback the updated value.
	if (!Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences())
	{
		UObject* Object = Get(true);

		Ar << Object;

		if (Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences())
		{
			*this = Object;
		}
	}
}

