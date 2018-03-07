// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "Serialization/ReloadObjectArc.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "Serialization/ArchiveReplaceArchetype.h"

/** Constructor */
FReloadObjectArc::FReloadObjectArc()
: FArchiveUObject(), Reader(Bytes), Writer(Bytes), RootObject(NULL), InstanceGraph(NULL)
, bAllowTransientObjects(true), bInstanceSubobjectsOnLoad(true)
{
}

/** Destructor */
FReloadObjectArc::~FReloadObjectArc()
{
	if ( InstanceGraph != NULL )
	{
		delete InstanceGraph;
		InstanceGraph = NULL;
	}
}

/**
 * Sets the root object for this memory archive.
 * 
 * @param	NewRoot		the UObject that should be the new root
 */
void FReloadObjectArc::SetRootObject( UObject* NewRoot )
{
	if ( NewRoot != NULL && InstanceGraph == NULL )
	{
		// if we are setting the top-level root object and we don't yet have an instance graph, create one
		InstanceGraph = new FObjectInstancingGraph(NewRoot);
		if ( IsLoading() )
		{
			// if we're reloading data onto objects, pre-initialize the instance graph with the objects instances that were serialized
			for ( int32 ObjectIndex = 0; ObjectIndex < CompleteObjects.Num(); ObjectIndex++ )
			{
				UObject* InnerObject = CompleteObjects[ObjectIndex];

				// ignore previously saved objects that aren't contained within the current root object
				if ( InnerObject->IsIn(NewRoot) )
				{
					InstanceGraph->AddNewInstance(InnerObject);
				}
			}
		}
	}

	RootObject = NewRoot;
	if ( RootObject == NULL && InstanceGraph != NULL )
	{
		// if we have cleared the top-level root object and we have an instance graph, delete it
		delete InstanceGraph;
		InstanceGraph = NULL;
	}
}

/**
 * Begin serializing a UObject into the memory archive.  Note that this archive does not use
 * object marks (OBJECTMARK_TagExp|OBJECTMARK_TagImp) to prevent recursion, as do most other archives that perform
 * similar functionality.  This is because this archive is used in operations that modify those
 * object flags for other purposes.
 *
 * @param	Obj		the object to serialize
 */
void FReloadObjectArc::SerializeObject( UObject* Obj )
{
	if ( Obj != NULL )
	{
		TSet<UObject*>& ObjectList = IsLoading()
			? LoadedObjects
			: SavedObjects;

		// only process this top-level object once
		if ( !ObjectList.Contains(Obj) )
		{
			ObjectList.Add(Obj);

			// store the current value of RootObject, in case our caller set it before calling SerializeObject
			UObject* PreviousRoot = RootObject;

			// set the root object to this object so that we load complete object data
			// for any objects contained within the top-level object (such as components)
			SetRootObject(Obj);

			// set this to prevent recursion in serialization
			if ( IsLoading() )
			{
				if (InstanceGraph != NULL)
				{
					// InitProperties will call CopyCompleteValue for any instanced object properties, so disable object instancing
					// because we probably already have an object that will be copied over that value; for any instanced object properties which
					// did not have a value when we were saving object data, we'll call InstanceSubobjects to instance those.  Also disable component
					// instancing as this object may not have the correct values for its component properties until its serialized, which result in
					// possibly creating lots of unnecessary components that will be thrown away anyway
					InstanceGraph->EnableSubobjectInstancing(false);
				}
				if ( Obj->GetClass() != UPackage::StaticClass() )
				{
					Obj->ReinitializeProperties(NULL, InstanceGraph);
				}
			}

			if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
			{
				Obj->GetClass()->SerializeDefaultObject(Obj, *this);
			}
			else
			{
				// save / load the data for this object that is different from its class
				Obj->Serialize(*this);
			}

			if ( IsLoading() )
			{
				if ( InstanceGraph != NULL )
				{
					InstanceGraph->EnableSubobjectInstancing(true);

					if ( bInstanceSubobjectsOnLoad )
					{
						// components which were identical to their archetypes weren't stored into this archive's object data, so re-instance those components now
						Obj->InstanceSubobjectTemplates(InstanceGraph);
					}
				}

				if ( !Obj->HasAnyFlags(RF_ClassDefaultObject) )
				{
					// allow the object to perform any cleanup after re-initialization
					Obj->PostLoad();
				}
			}

			// restore the RootObject - we're done with it.
			SetRootObject(PreviousRoot);
		}
	}
}

/**
 * Resets the archive so that it can be loaded from again from scratch
 * as if it was never serialized as a Reader
 */
void FReloadObjectArc::Reset()
{
	// empty the list of objects that were loaded, so we can load again
	LoadedObjects.Empty();
	// reset our location in the buffer
	Seek(0);
}


/**
 * I/O function for FName
 */
FArchive& FReloadObjectArc::operator<<( class FName& Name )
{
	NAME_INDEX NameComparisonIndex;
	NAME_INDEX NameDisplayIndex;
	int32 NameInstance;
	if ( IsLoading() )
	{
		Reader << NameComparisonIndex << NameDisplayIndex << NameInstance;

		// recreate the FName using the serialized index and instance number
		Name = FName(NameComparisonIndex, NameDisplayIndex, NameInstance);
	}
	else if ( IsSaving() )
	{
		NameComparisonIndex = Name.GetComparisonIndex();
		NameDisplayIndex = Name.GetDisplayIndex();
		NameInstance = Name.GetNumber();

		Writer << NameComparisonIndex << NameDisplayIndex << NameInstance;
	}
	return *this;
}

/**
 * I/O function for UObject references
 */
FArchive& FReloadObjectArc::operator<<( class UObject*& Obj )
{
	if ( IsLoading() )
	{
		int32 Index = 0;
		Reader << Index;

		// An index of 0 indicates that the value of the object was NULL
		if ( Index == 0 )
		{
			Obj = NULL;
		}
		else if ( Index < 0 )
		{
			// An index less than zero indicates an object for which we only stored the object pointer
			Obj = ReferencedObjects[-Index-1];
		}
		else if ( Index > 0 )
		{
			// otherwise, the memory archive contains the entire UObject data for this UObject, so load
			// it from the archive
			Obj = CompleteObjects[Index-1];

			// Ensure we don't load it more than once.
			if ( !LoadedObjects.Contains(Obj) )
			{
				LoadedObjects.Add(Obj);

				// find the location for this UObject's data in the memory archive
				int32* ObjectOffset = ObjectMap.Find(Obj);
				checkf(ObjectOffset,TEXT("%s wasn't not found in ObjectMap for %s - double-check that %s (and objects it references) saves the same amount of data it loads if using custom serialization"),
					*Obj->GetFullName(), *GetArchiveName(), *RootObject->GetFullName());

				// move the reader to that position
				Reader.Seek(*ObjectOffset);

				// make sure object instancing is disabled before calling ReinitializeProperties; otherwise new copies of objects will be
				// created for any instanced object properties owned by this object and then immediately overwritten when its serialized
				InstanceGraph->EnableSubobjectInstancing(false);

				// Call ReinitializeProperties to propagate base change to 'within' objects (like Components).
				Obj->ReinitializeProperties( NULL, InstanceGraph );

				// read in the data for this UObject
				Obj->Serialize(*this);

				// we should never have RootObject serialized as an object contained by the root object
				checkSlow(Obj != RootObject);

				// serializing the stored data for this object should have replaced all of its original instanced object references
				// but there might have been new subobjects added to the object's archetype in the meantime (in the case of updating 
				// an prefab from a prefab instance, for example), so enable subobject instancing and instance those now.
				InstanceGraph->EnableSubobjectInstancing(true);

				if ( bInstanceSubobjectsOnLoad )
				{
					// components which were identical to their archetypes weren't stored into this archive's object data, so re-instance those components now
					Obj->InstanceSubobjectTemplates(InstanceGraph);
				}

				// normally we'd never have CDOs in the list of CompleteObjects (CDOs can't be contained within other objects)
				// but in some cases this operator is invoked directly (prefabs)
				if ( !Obj->HasAnyFlags(RF_ClassDefaultObject) )
				{
					Obj->PostLoad();
				}

			}
		}
	}
	else if ( IsSaving() )
	{
		// Don't save references to transient or deleted objects.
		if ( Obj == NULL || (Obj->HasAnyFlags(RF_Transient) && !bAllowTransientObjects) || Obj->IsPendingKill() )
		{
			// null objects are stored as 0 indexes
			int32 Index = 0;
			Writer << Index;
			return *this;
		}

		// See if we have already written this object out.
		int32 CompleteIndex = CompleteObjects.Find(Obj);
		int32 ReferencedIndex = ReferencedObjects.Find(Obj);

		// The same object can't be in both arrays.
		check( !(CompleteIndex != INDEX_NONE && ReferencedIndex != INDEX_NONE) );

		if(CompleteIndex != INDEX_NONE)
		{
			int32 Index = CompleteIndex + 1;
			Writer << Index;
		}
		else if(ReferencedIndex != INDEX_NONE)
		{
			int32 Index = -ReferencedIndex - 1;
			Writer << Index;
		}
		// New object - was not already saved.
		// if the object is in the SavedObjects array, it means that the object was serialized into this memory archive as a root object
		// in this case, just serialize the object as a simple reference
		else if ( Obj->IsIn(RootObject) && !SavedObjects.Contains(Obj) )
		{
			SavedObjects.Add(Obj);

			// we need to store the UObject data for this object
			check(ObjectMap.Find(Obj) == NULL);

			// only the location of the UObject in the CompleteObjects
			// array is stored in the memory archive, using FPackageIndex-style
			// notation
			int32 Index = CompleteObjects.AddUnique(Obj) + 1;
			Writer << Index;

			// remember the location of the beginning of this UObject's data
			ObjectMap.Add(Obj,Writer.Tell());

			Obj->Serialize(*this);
		}
		else
		{
			// Referenced objects will be indicated by negative indexes
			int32 Index = -ReferencedObjects.AddUnique(Obj) - 1;
			Writer << Index;
		}
	}
	return *this;
}

/*----------------------------------------------------------------------------
	FArchiveReplaceArchetype.
----------------------------------------------------------------------------*/
FArchiveReplaceArchetype::FArchiveReplaceArchetype()
: FReloadObjectArc()
{
	// don't wipe out transient objects
	bAllowTransientObjects = true;

	// setting this flag indicates that component references should only be serialized into this archive if there the component has different values that its archetype,
	// and only when the component is being compared to its archetype (as opposed to another component instance, for example)
	SetPortFlags(PPF_DeepCompareInstances);
}
