// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

/**
 * Archive for reloading UObjects without requiring the UObject to be completely unloaded.
 * Used for easily repropagating defaults to UObjects after class data has been changed.
 * Aggregates FMemoryReader and FMemoryWriter to encapsulate management and coordination 
 * of the UObject data being saved/loaded.
 * <p>
 * UObject references are not serialized directly into the memory archive.  Instead, we use
 * a system similar to the Export/ImportMap of FLinker - the pointer to the UObject is added
 * to a persistent (from the standpoint of the FReloadObjectArc) array.  The location into
 * this array is what is actually stored in the archive's buffer.
 * <p>
 * This is primarily necessary for loading UObject references from the memory archive.  The
 * UObject pointer ref. passed into the UObject overloaded serialization operator is not
 * guaranteed to be valid when reading data from the memory archive.  Since we need to determine
 * whether we *should* serialize the object before we actually do, we must introduce a level of
 * indirection.  Using the index retrieved from the archive's buffer, we can look at the UObject
 * before we attempt to serialize it.
 */
class COREUOBJECT_API FReloadObjectArc : public FArchiveUObject
{
public:

	/**
	 * Changes this memory archive to "read" mode, for reading UObject
	 * data from the temporary location back into the UObjects.
	 */
	void ActivateReader()
	{
		ArIsSaving = false;
		ArIsLoading = true;
	}

	/**
	 * Changes this memory archive to "write" mode, for storing UObject
	 * data to the temporary location.
	 *
	 * @note: called from ctors in child classes - should never be made virtual
	 */
	void ActivateWriter()
	{
		ArIsSaving = true;
		ArIsLoading = false;
	}

	/**
	 * Begin serializing a UObject into the memory archive.
	 *
	 * @param	Obj		the object to serialize
	 */
	void SerializeObject( UObject* Obj );

	/**
	 * Resets the archive so that it can be loaded from again from scratch
	 * as if it was never serialized as a Reader
	 */
	void Reset();

	/** FArchive Interface */
	int64 TotalSize()
	{
		return Bytes.Num();
	}
	void Seek( int64 InPos )
	{
		if ( IsLoading() )
			Reader.Seek(InPos);
		else if ( IsSaving() )
			Writer.Seek(InPos);
	}
	int64 Tell()
	{
		return IsLoading() ? Reader.Tell() : Writer.Tell();
	}
	FArchive& operator<<( class FName& Name );
	FArchive& operator<<(class UObject*& Obj);

	/** Constructor */
	FReloadObjectArc();

	/** Destructor */
	virtual ~FReloadObjectArc();

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FReloadObjectArc"); }

protected:

	/**
	 * Raw I/O function.  Routes the call to the appropriate archive, depending on whether
	 * we're loading/saving.
	 */
	void Serialize( void* Data, int64 Num )
	{
		if ( IsLoading() )
		{
			Reader.Serialize(Data, Num);
		}
		else if ( IsSaving() )
		{
			Writer.Serialize(Data,Num);
		}
	}

	/**
	 * Sets the root object for this memory archive.
	 * 
	 * @param	NewRoot		the UObject that should be the new root
	 */
	void SetRootObject( UObject* NewRoot );

	/** the raw UObject data contained by this archive */
	TArray<uint8>		Bytes;

	/** moves UObject data from storage into UObject address space */
	FMemoryReader		Reader;

	/** stores UObject data in a temporary location for later retrieval */
	FMemoryWriter		Writer;

	/** UObjects for which all data is stored in the memory archive */
	TArray<UObject*>	CompleteObjects;

	/** UObjects for which only a reference to the object is stored in the memory archive */
	TArray<UObject*>	ReferencedObjects;

	/**
	 * List of top-level objects that have saved into the memory archive.  Used to prevent objects
	 * from being serialized into storage multiple times.
	 */
	TSet<UObject*>	SavedObjects;

	/**
	 * List of top-level objects that have been loaded using the memory archive.  Used to prevent
	 * objects from being serialized multiple times from the same memory archive.
	 */
	TSet<UObject*>	LoadedObjects;

	/** A mapping of "UObject" => "the offset for that UObject's data in the Bytes array" for the objects stored in this archive */
	TMap<UObject*,int32>	ObjectMap;

	/**
	 * This is the current top-level object.  For any UObjects contained
	 * within this object, the complete UObject data will be stored in the
	 * archive's buffer
	 */
	UObject*			RootObject;

	/**
	 * Used for tracking the subobjects and components that are instanced during this object reload.
	 */
	struct FObjectInstancingGraph*	InstanceGraph;

	/**
	 * Indicates whether this archive will serialize references to objects with the RF_Transient flag. (defaults to false)
	 */
	bool				bAllowTransientObjects;

	/**
	 * Indicates whether this archive should call InstanceSubobjectTemplates/InstanceSubobjects on objects that it re-initializes.
	 * Specify false if the object needs special handling before calling InstanceSubobjectTemplates
	 */
	bool				bInstanceSubobjectsOnLoad;
};
