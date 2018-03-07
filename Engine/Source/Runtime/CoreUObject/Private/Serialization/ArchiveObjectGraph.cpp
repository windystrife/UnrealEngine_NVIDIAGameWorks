// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveObjectGraph.h"
#include "UObject/UObjectIterator.h"

// This is from FArchiveTraceRoute -This only creates object graph of all objects 
// This can be used by other classes such as FTraceReferences - trace references of one object
FArchiveObjectGraph::FArchiveObjectGraph(bool IncludeTransients, EObjectFlags	KeepFlags)
:	CurrentReferencer(NULL),
	bIncludeTransients(IncludeTransients), 
	RequiredFlags(KeepFlags)
{
	ArIsObjectReferenceCollector = true;

	// ALL objects reference their outers...it's just log spam here
	//ArIgnoreOuterRef = true;

	TArray<UObject*> RootObjects;

	// allocate enough memory for all objects
	ObjectGraph.Empty(GUObjectArray.GetObjectArrayNum());
	RootObjects.Empty(GUObjectArray.GetObjectArrayNum());

	// search for objects that have the right flags and add them to the list of objects that we're going to start with
	// all other objects need to be tagged so that we can tell whether they've been serialized or not.
	for( FObjectIterator It; It; ++It )
	{
		UObject* CurrentObject = *It;
		if ( CurrentObject->HasAnyFlags(RequiredFlags) )
		{
			// make sure it isn't tagged
			// ASKRON: WHY do we need this?
			CurrentObject->UnMark(OBJECTMARK_TagExp);
			RootObjects.Add(CurrentObject);
			ObjectGraph.Add(CurrentObject, new FObjectGraphNode(CurrentObject));
		}
		else
		{
			// ASKRON: WHY do we need this?
			CurrentObject->Mark(OBJECTMARK_TagExp);
		}
	}

	// Populate the ObjectGraph - this serializes our root set to map out the relationships between all rooted objects
	GenerateObjectGraph(RootObjects);

	// we won't be adding any additional objects for the arrays and graphs, so free up any memory not being used.
	RootObjects.Shrink();
	ObjectGraph.Shrink();

	// we're done with serialization; clear the tags so that we don't interfere with anything else
	for( FObjectIterator It; It; ++It )
	{
		It->UnMark(OBJECTMARK_TagExp);
	}
}

FArchiveObjectGraph::~FArchiveObjectGraph()
{
	for ( TMap<UObject*, FObjectGraphNode*>::TIterator It(ObjectGraph); It; ++It )
	{
		delete It.Value();
		It.Value() = NULL;
	}
}

	/** Handles serialization of UObject references */
FArchive& FArchiveObjectGraph::operator<<( class UObject*& Obj )
{
	if ( Obj != NULL
		&&	(bIncludeTransients || !Obj->HasAnyFlags(RF_Transient)) )
	{
		// grab the object graph node for this object and its referencer, creating them if necessary
		FObjectGraphNode* CurrentObjectNode = ObjectGraph.FindRef(Obj);
		if ( CurrentObjectNode == NULL )
		{
			CurrentObjectNode = ObjectGraph.Add(Obj, new FObjectGraphNode(Obj));
		}
		FObjectGraphNode* ReferencerNode = ObjectGraph.FindRef(CurrentReferencer);
		if ( ReferencerNode == NULL )
		{
			ReferencerNode = ObjectGraph.Add(CurrentReferencer, new FObjectGraphNode(CurrentReferencer));
		}

		if ( Obj != CurrentReferencer )
		{
			FTraceRouteRecord * Record = ReferencerNode->ReferencedObjects.Find(Obj);
			// now record the references between this object and the one referencing it
			if ( !Record )
			{
				ReferencerNode->ReferencedObjects.Add(Obj, FTraceRouteRecord(CurrentObjectNode, GetSerializedProperty()));
			}
			else
			{
				Record->Add(GetSerializedProperty());
			}

			Record = CurrentObjectNode->ReferencerRecords.Find(CurrentReferencer);
			if ( !Record )
			{
				CurrentObjectNode->ReferencerRecords.Add(CurrentReferencer, FTraceRouteRecord(ReferencerNode, GetSerializedProperty()));
			}
			else
			{
				Record->Add(GetSerializedProperty());
			}
		}

		// if this object is still tagged for serialization, add it to the list
		if ( Obj->HasAnyMarks(OBJECTMARK_TagExp) )
		{
			Obj->UnMark(OBJECTMARK_TagExp);
			ObjectsToSerialize.Add(Obj);
		}
	}
	return *this;
}

void FArchiveObjectGraph::GenerateObjectGraph( TArray<UObject*>& Objects )
{
	const int32 LastRootObjectIndex = Objects.Num();

	for ( int32 ObjIndex = 0; ObjIndex < Objects.Num(); ObjIndex++ )
	{
		CurrentReferencer = Objects[ObjIndex];
		CurrentReferencer->UnMark(OBJECTMARK_TagExp);

		// Serialize this object
		if ( CurrentReferencer->HasAnyFlags(RF_ClassDefaultObject) )
		{
			CurrentReferencer->GetClass()->SerializeDefaultObject(CurrentReferencer, *this);
		}
		else
		{
			CurrentReferencer->Serialize( *this );
		}

		// ObjectsToSerialize will contain only those objects which were encountered while serializing CurrentReferencer
		// that weren't already in the list of objects to be serialized.
		if ( ObjectsToSerialize.Num() > 0 )
		{
			// add to objects, so that we make sure ObjectToSerialize are serialized
			Objects += ObjectsToSerialize;
			ObjectsToSerialize.Empty();
		}
	}

	Objects.RemoveAt(LastRootObjectIndex, Objects.Num() - LastRootObjectIndex);
}

void FArchiveObjectGraph::ClearSearchFlags()
{
	for ( TMap<UObject*, FObjectGraphNode*>::TIterator Iter(ObjectGraph); Iter; ++Iter )
	{
		FObjectGraphNode * GraphNode = Iter.Value();
		if ( GraphNode )
		{
			GraphNode->Visited = 0;
			GraphNode->ReferenceDepth = MAX_int32;
			GraphNode->ReferencerProperties.Empty();
		}
	}
}
