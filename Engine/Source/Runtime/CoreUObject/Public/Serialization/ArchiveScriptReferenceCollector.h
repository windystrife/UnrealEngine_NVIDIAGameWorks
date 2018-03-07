// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Serialization/ArchiveUObject.h"

/*******************************************************************************
 * FArchiveScriptReferenceCollector
 ******************************************************************************/

class FArchiveScriptReferenceCollector : public FArchiveUObject
{
public:
	/**
	* Constructor
	*
	* @param	InObjectArray			Array to add object references to
	*/
	FArchiveScriptReferenceCollector(TArray<UObject*>& InObjectArray)
		: ObjectArray(InObjectArray)
	{
		ArIsObjectReferenceCollector = true;
		ArIsPersistent = false;
		ArIgnoreArchetypeRef = false;
	}
protected:
	/**
	* UObject serialize operator implementation
	*
	* @param Object	reference to Object reference
	* @return reference to instance of this class
	*/
	FArchive& operator<<(UObject*& Object)
	{
		// Avoid duplicate entries.
		if (Object != NULL && !ObjectArray.Contains(Object))
		{
			check(Object->IsValidLowLevel());
			ObjectArray.Add(Object);
		}
		return *this;
	}

	/** Stored reference to array of objects we add object references to */
	TArray<UObject*>&		ObjectArray;
};
