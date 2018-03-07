// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveUObject.h"

/*----------------------------------------------------------------------------
	FFindReferencersArchive.
----------------------------------------------------------------------------*/
/**
 * Archive for mapping out the referencers of a collection of objects.
 */
class FFindReferencersArchive : public FArchiveUObject
{
public:
	/**
	 * Constructor
	 *
	 * @param	PotentialReferencer		the object to serialize which may contain references to our target objects
	 * @param	InTargetObjects			array of objects to search for references to
	 * @param	bFindAlsoWeakReferences should we also look into weak references?
	 */
	COREUOBJECT_API FFindReferencersArchive(class UObject* PotentialReferencer, const TArray<class UObject*>& InTargetObjects, bool bFindAlsoWeakReferences = false);

	/**
	 * Retrieves the number of references from PotentialReferencer to the object specified.
	 *
	 * @param	TargetObject	the object to might be referenced
	 * @param	out_ReferencingProperties
	 *							receives the list of properties which were holding references to TargetObject
	 *
	 * @return	the number of references to TargetObject which were encountered when PotentialReferencer
	 *			was serialized.
	 */
	COREUOBJECT_API int32 GetReferenceCount( class UObject* TargetObject, TArray<class UProperty*>* out_ReferencingProperties=NULL ) const;

	/**
	 * Retrieves the number of references from PotentialReferencer list of TargetObjects
	 *
	 * @param	out_ReferenceCounts		receives the number of references to each of the TargetObjects
	 *
	 * @return	the number of objects which were referenced by PotentialReferencer.
	 */
	COREUOBJECT_API int32 GetReferenceCounts( TMap<class UObject*, int32>& out_ReferenceCounts ) const;

	/**
	 * Retrieves the number of references from PotentialReferencer list of TargetObjects
	 *
	 * @param	out_ReferenceCounts			receives the number of references to each of the TargetObjects
	 * @param	out_ReferencingProperties	receives the map of properties holding references to each referenced object.
	 *
	 * @return	the number of objects which were referenced by PotentialReferencer.
	 */
	COREUOBJECT_API int32 GetReferenceCounts( TMap<class UObject*, int32>& out_ReferenceCounts, TMultiMap<class UObject*,class UProperty*>& out_ReferencingProperties ) const;

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	COREUOBJECT_API virtual FString GetArchiveName() const { return TEXT("FFindReferencersArchive"); }

	/**
	 * Resets the reference counts.  Keeps the same target objects but sets up everything to test a new potential referencer.
	 * @param	PotentialReferencer		the object to serialize which may contain references to our target objects
	 **/
	COREUOBJECT_API void ResetPotentialReferencer(UObject* InPotentialReferencer);

protected:
	TMap<class UObject*, int32>	TargetObjects;

	/** a mapping of target object => the properties in PotentialReferencer that hold the reference to target object */
	TMultiMap<class UObject*,class UProperty*> ReferenceMap;

	/** The potential referencer we ignore */
	class UObject* PotentialReferencer;

private:

	/**
	 * Serializer - if Obj is one of the objects we're looking for, increments the reference count for that object
	 */
	COREUOBJECT_API FArchive& operator<<( class UObject*& Obj );
};
