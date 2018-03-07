// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveUObject.h"

/*----------------------------------------------------------------------------
	FArchiveFindCulprit.
----------------------------------------------------------------------------*/
/**
 * Archive for finding who references an object.
 */
class FArchiveFindCulprit : public FArchiveUObject
{
public:
	/**
	 * Constructor
	 *
	 * @param	InFind	the object that we'll be searching for references to
	 * @param	Src		the object to serialize which may contain a reference to InFind
	 * @param	InPretendSaving		if true, marks the archive as saving and persistent, so that a different serialization codepath is followed
	 */
	COREUOBJECT_API FArchiveFindCulprit( UObject* InFind, UObject* Src, bool InPretendSaving );

	int32 GetCount() const
	{
		return Count;
	}
	int32 GetCount( TArray<const UProperty*>& Properties )
	{
		Properties = Referencers;
		return Count;
	}

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FArchiveFindCulprit"); }

protected:
	UObject*			Find;
	int32					Count;
	bool				PretendSaving;
	class TArray<const UProperty*>	Referencers;

private:
	COREUOBJECT_API FArchive& operator<<( class UObject*& Obj );
};
