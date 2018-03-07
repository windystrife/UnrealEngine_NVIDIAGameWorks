// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveTraceRoute.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Containers/List.h"
#include "UObject/UnrealType.h"

/*----------------------------------------------------------------------------
	FArchiveTraceRoute
----------------------------------------------------------------------------*/

TMap<UObject*,UProperty*> FArchiveTraceRoute::FindShortestRootPath( UObject* Obj, bool bIncludeTransients, EObjectFlags KeepFlags )
{
	// Take snapshot of object flags that will be restored once marker goes out of scope.
	FScopedObjectFlagMarker ObjectFlagMarker;

	TMap<UObject*,FTraceRouteRecord> Routes;
	FArchiveTraceRoute Rt( Obj, Routes, bIncludeTransients, KeepFlags );

	TMap<UObject*,UProperty*> Result;

	// No routes are reported if the object wasn't rooted.
	if ( Routes.Num() > 0 || Obj->HasAnyFlags(KeepFlags) )
	{
		TArray<FTraceRouteRecord> Records;
		Routes.GenerateValueArray(Records);

		// the target object is NOT included in the result, so add it first.  Then iterate over the route
		// backwards, following the trail from the target object to the root object.
		Result.Add(Obj, NULL);
		for ( int32 RecordIndex = Records.Num() - 1; RecordIndex >= 0; RecordIndex-- )
		{
			FTraceRouteRecord& Record = Records[RecordIndex];
			// To keep same behavior as previous, it will set last one for now until Ron comes back
			for ( int32 ReferenceIndex = 0; ReferenceIndex<Record.ReferencerProperties.Num(); ++ReferenceIndex )
			{
				if (Record.ReferencerProperties[ReferenceIndex])
				{
					Result.Add( Record.GraphNode->NodeObject, Record.ReferencerProperties[ReferenceIndex]);
					break;
				}
			}
		}
	}

#if 0
	// eventually we'll merge this archive with the FArchiveFindCulprit, since we have all the same information already
	// but before we can do so, we'll need to change the TraceRouteRecord's ReferencerProperty to a TArray so that we
	// can report when an object has multiple references to another object
	FObjectGraphNode* ObjNode = Rt.ObjectGraph.FindRef(Obj);
	if ( ObjNode != NULL && ObjNode->ReferencerRecords.Num() > 0 )
	{
		UE_LOG(LogSerialization, Log,  TEXT("Referencers of %s (from FArchiveTraceRoute):"), *Obj->GetFullName() );
		int32 RefIndex=0;
		for ( TMap<UObject*,FTraceRouteRecord>::TIterator It(ObjNode->ReferencerRecords); It; ++It )
		{
			UObject* Object = It.Key();
			FTraceRouteRecord& Record = It.Value();
			UE_LOG(LogSerialization, Log, TEXT("      %i) %s  (%s)"), RefIndex++, *Object->GetFullName(), *Record.ReferencerProperty->GetFullName());
		}
	}
#endif

	return Result;
}

/**
 * Retuns path to root created by e.g. FindShortestRootPath via a string.
 *
 * @param TargetObject	object marking one end of the route
 * @param Route			route to print to log.
 * @param String of root path
 */
FString FArchiveTraceRoute::PrintRootPath( const TMap<UObject*,UProperty*>& Route, const UObject* TargetObject )
{
	FString RouteString;
	for( TMap<UObject*,UProperty*>::TConstIterator MapIt(Route); MapIt; ++MapIt )
	{
		UObject*	Object		= MapIt.Key();
		UProperty*	Property	= MapIt.Value();

		FString	ObjectReachability;
		
		if( Object == TargetObject )
		{
			ObjectReachability = TEXT(" [target]");
		}
		
		if( Object->IsRooted() )
		{
			ObjectReachability += TEXT(" (root)");
		}
		
		if( Object->IsNative() )
		{
			ObjectReachability += TEXT(" (native)");
		}
		
		if( Object->HasAnyFlags(RF_Standalone) )
		{
			ObjectReachability += TEXT(" (standalone)");
		}
		
		if( ObjectReachability == TEXT("") )
		{
			ObjectReachability = TEXT(" ");
		}
			
		FString ReferenceSource;
		if( Property != NULL )
		{
			ReferenceSource = FString::Printf(TEXT("%s (%s)"), *ObjectReachability, *Property->GetFullName());
		}
		else
		{
			ReferenceSource = ObjectReachability;
		}

		RouteString += FString::Printf(TEXT("   %s%s%s"), *Object->GetFullName(), *ReferenceSource, LINE_TERMINATOR );
	}

	if( !Route.Num() )
	{
		RouteString = TEXT("   (Object is not currently rooted)\r\n");
	}
	return RouteString;
}

FArchiveTraceRoute::FArchiveTraceRoute( UObject* TargetObject, TMap<UObject*,FTraceRouteRecord>& InRoutes, bool bShouldIncludeTransients, EObjectFlags KeepFlags )
:	CurrentReferencer(NULL)
,	Depth(0)
,	bIncludeTransients(bShouldIncludeTransients)
,	RequiredFlags(KeepFlags)
{
	// this object is part of the root set; don't have to do anything
	if ( TargetObject == NULL || TargetObject->HasAnyFlags(KeepFlags) )
	{
		return;
	}

	ArIsObjectReferenceCollector = true;
	
	TSparseArray<UObject*> RootObjects;

	// allocate enough memory for all objects
	ObjectGraph.Empty(GUObjectArray.GetObjectArrayNum());
	RootObjects.Empty(GUObjectArray.GetObjectArrayNum() / 2);

	// search for objects that have the right flags and add them to the list of objects that we're going to start with
	// all other objects need to be tagged so that we can tell whether they've been serialized or not.
	for( FObjectIterator It; It; ++It )
	{
		UObject* CurrentObject = *It;
		if ( CurrentObject->HasAnyFlags(RequiredFlags) || CurrentObject->IsRooted() )
		{
			// make sure it isn't tagged
			CurrentObject->UnMark(OBJECTMARK_TagExp);
			RootObjects.Add(CurrentObject);
			ObjectGraph.Add(CurrentObject, new FObjectGraphNode(CurrentObject));
		}
		else
		{
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

	// Now we calculate the shortest path from the target object to a rooted object; if the target object isn't
	// in the object graph, it means it isn't rooted.
	FObjectGraphNode* ObjectNode = ObjectGraph.FindRef(TargetObject);
	if ( ObjectNode )
	{
		ObjectNode->ReferenceDepth = 0;

		// This method sets up the ReferenceDepth values for all relevant nodes
		CalculateReferenceDepthsForNode(ObjectNode);

		int32 LowestDepth = MAX_int32;
		FRouteLink ClosestLink;

		// Next, we find the root object that has the lowest Depth value
		//@fixme - we might have multiple root objects which have the same depth value
//	 	TArray<TDoubleLinkedList<FObjectGraphNode*> > ShortestRoutes;
		TDoubleLinkedList<FObjectGraphNode*> ShortestRoute;
		for ( int32 RootObjectIndex = 0; RootObjectIndex < RootObjects.Num(); RootObjectIndex++ )
		{
			FObjectGraphNode* RootObjectNode = ObjectGraph.FindRef(RootObjects[RootObjectIndex]);
			FindClosestLink(RootObjectNode, LowestDepth, ClosestLink);
		}

		// At this point, we should know which root object has the shortest depth from the target object.  Push that
		// link into our linked list and recurse into the links to navigate our way through the reference chain to the
		// target object.
		ShortestRoute.AddHead(ClosestLink.LinkParent);
		if ( ClosestLink.LinkChild != NULL )
		{
			ShortestRoute.AddTail(ClosestLink.LinkChild);
			while ( FindClosestLink(ClosestLink.LinkChild, LowestDepth, ClosestLink)/* || LowestDepth > 0*/ )
			{
				// at this point, LinkChild will be different than it was when we last evaluated the conditional
				ShortestRoute.AddTail(ClosestLink.LinkChild);
			}
		}

		// since we know that the target object is rooted, there should be at least one route to a root object;
		// therefore LowestDepth should always be zero or there is a bug somewhere...
		check(LowestDepth==0);

		// Finally, fill in the output value - it will start with the rooted object and follow the chain of object references
		// to the target object.  However, the node for the target object itself is NOT included in this result.
		for (TDoubleLinkedList<FObjectGraphNode*>::TIterator It(ShortestRoute.GetHead()); It; ++It )
		{
			FObjectGraphNode* CurrentNode = *It;
			InRoutes.Add(CurrentNode->NodeObject, FTraceRouteRecord(CurrentNode, CurrentNode->ReferencerProperties));
		}
	}
}

/**
 * Searches through the objects referenced by CurrentNode for a record with a Depth lower than LowestDepth.
 *
 * @param	CurrentNode		the node containing the list of referenced objects that will be searched.
 * @param	LowestDepth		the current number of links we are from the target object.
 * @param	ClosestLink		if a trace route record is found with a lower depth value than LowestDepth, the link is saved to this value.
 *
 * @return	true if a closer link was discovered; false if no links were closer than lowest depth, or if we've reached the target object.
 */
bool FArchiveTraceRoute::FindClosestLink( FObjectGraphNode* CurrentNode, int32& LowestDepth, FRouteLink& ClosestLink )
{
	bool bResult = false;

	if ( CurrentNode != NULL )
	{
		for ( TMap<UObject*, FTraceRouteRecord>::TIterator RefIt(CurrentNode->ReferencedObjects); RefIt; ++RefIt )
		{
			FTraceRouteRecord& Record = RefIt.Value();

			// a ReferenceDepth of MAX_int32 means that this object was not part of the target object's reference graph
			if ( Record.GraphNode->ReferenceDepth < MAX_int32 )
			{
				if ( Record.GraphNode->ReferenceDepth == 0 )
				{
					// found the target
					if ( CurrentNode->ReferenceDepth < LowestDepth )
					{
						// the target object is referenced directly by a rooted object
						ClosestLink = FRouteLink(CurrentNode, NULL);
					}
					LowestDepth = CurrentNode->ReferenceDepth - 1;
					bResult = false;
					break;
				}
				else if ( Record.GraphNode->ReferenceDepth < LowestDepth )
				{
					LowestDepth = Record.GraphNode->ReferenceDepth;
					ClosestLink = FRouteLink(CurrentNode, Record.GraphNode);
					bResult = true;
				}
				else if ( Record.GraphNode->ReferenceDepth == LowestDepth )
				{
					// once we've changed this to an array, push this link on
				}
			}
		}
	}

	return bResult;
}

/**
 * Destructor.  Deletes all FObjectGraphNodes created by this archive.
 */
FArchiveTraceRoute::~FArchiveTraceRoute()
{
	for ( TMap<UObject*, FObjectGraphNode*>::TIterator It(ObjectGraph); It; ++It )
	{
		delete It.Value();
		It.Value() = NULL;
	}
}

FArchive& FArchiveTraceRoute::operator<<( class UObject*& Obj )
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

/**
 * Serializes the objects in the specified set; any objects encountered during serialization
 * of an object are added to the object set and processed until no new objects are added.
 *
 * @param	Objects		the original set of objects to serialize; the original set will be preserved.
 */
void FArchiveTraceRoute::GenerateObjectGraph( TSparseArray<UObject*>& Objects )
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
			Objects += ObjectsToSerialize;
			ObjectsToSerialize.Empty();
		}
	}

	Objects.RemoveAt(LastRootObjectIndex, Objects.Num() - LastRootObjectIndex);
}

/**
 * Recursively iterates over the referencing objects for the specified node, marking each with
 * the current Depth value.  Stops once it reaches a route root.
 *
 * @param	ObjectNode	the node to evaluate.
 */
void FArchiveTraceRoute::CalculateReferenceDepthsForNode( FObjectGraphNode* ObjectNode )
{
	check(ObjectNode);

	Depth++;

	TSparseArray<FObjectGraphNode*> RecurseRecords;
	// for each referencer, set the current depth.  Do this before recursing into this object's
	// referencers to minimize the number of times we have to revisit nodes
	for ( TMap<UObject*, FTraceRouteRecord>::TIterator It(ObjectNode->ReferencerRecords); It; ++It )
	{
		FTraceRouteRecord& Record = It.Value();

		checkSlow(Record.GraphNode);
		if ( Record.GraphNode->ReferenceDepth > Depth )
		{
			Record.GraphNode->ReferenceDepth = Depth;
			Record.GraphNode->ReferencerProperties.Append(Record.ReferencerProperties);
			RecurseRecords.Add(Record.GraphNode);
		}
	}

	for ( TSparseArray<FObjectGraphNode*>::TIterator It(RecurseRecords); It; ++It )
	{
		FObjectGraphNode* CurrentNode = *It;
		It.RemoveCurrent();

		// this record may have been encountered by processing another node; if that resulted in a shorter
		// route for this record, just skip it.
		if ( CurrentNode->ReferenceDepth == Depth )
		{
			// if the object from this node has one of the required flags, don't process this object's referencers
			// as it's considered a "root" for the route
			if (!CurrentNode->NodeObject->HasAnyFlags(RequiredFlags) && !CurrentNode->NodeObject->IsRooted())
			{
				CalculateReferenceDepthsForNode(CurrentNode);
			}
		}
	}

	Depth--;
}
