// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Serialization/ArchiveUObject.h"

struct FTraceRouteRecord
{
	struct FObjectGraphNode*	GraphNode;
	TArray<UProperty*>			ReferencerProperties;

	FTraceRouteRecord( struct FObjectGraphNode* InGraphNode, UProperty* InReferencerProperty)
	: GraphNode(InGraphNode)
	{
		ReferencerProperties.Add(InReferencerProperty);
	}

	FTraceRouteRecord( struct FObjectGraphNode* InGraphNode, const TArray<UProperty*>&	InReferencerProperties )
		: GraphNode(InGraphNode)
	{
		ReferencerProperties = InReferencerProperties;
	}

	void Add(UProperty* InReferencerProperty)
	{
		ReferencerProperties.Add(InReferencerProperty);
	}
};

struct FObjectGraphNode
{
	/** the object this node represents */
	UObject*	NodeObject;

	/** Records for objects which reference this object */
	TMap<UObject*, FTraceRouteRecord>	ReferencerRecords;

	/** Records for objects which are referenced by this object */
	TMap<UObject*, FTraceRouteRecord>	ReferencedObjects;

	/** the number of links between NodeObject and the target object */
	int32									ReferenceDepth;

	/** Used during search - Visited or not */
	bool							Visited;

	/**
	 * The property that references NodeObject; only set on nodes which are part
	 * of the calculated shortest route
	 */
	TArray<UProperty*>							ReferencerProperties;

	/** Default constructor */
	FObjectGraphNode( UObject* InNodeObject=NULL )
	:	NodeObject(InNodeObject)
	,	ReferenceDepth(MAX_int32)
	,	Visited(false)
	{}
};

/*----------------------------------------------------------------------------
	FArchiveObjectGraph.
----------------------------------------------------------------------------*/
// This is from FArchiveTraceRoute -This only creates object graph of all objects 
// This can be used by other classes such as FTraceReferences - trace references of one object
class FArchiveObjectGraph : public FArchiveUObject
{
	/** Handles serialization of UObject references */
	FArchive& operator<<( class UObject*& Obj );

	/**
	* The object currently being serialized; used by the overloaded serialization operator to determine the referencing object.
	*/
	UObject* CurrentReferencer;
	/** The set of objects encountered while serializing CurrentReferencer */
	TArray<UObject*> ObjectsToSerialize;

	/** true if we should serialize objects marked RF_Transient */
	bool bIncludeTransients;

	/**
	* A bitmask of object flags which indicates which objects should be included in the group of initial objects to be serialized;
	* RF_RootSet will be automatically added to this bitmask, and OBJECTMARK_TagExp will automatically be removed.
	*/
	EObjectFlags	RequiredFlags;

public:
	FArchiveObjectGraph(bool IncludeTransients, EObjectFlags KeepFlags);
	~FArchiveObjectGraph();

	/**
	* Serializes the objects in the specified set; any objects encountered during serialization
	* of an object are added to the object set and processed until no new objects are added.
	* DO NOT MAKE THIS VIRTUAL - this is called by constructor. If you wish to do so, please change where be called
	* @param	Objects		the original set of objects to serialize; the original set will be preserved.
	*/
	void GenerateObjectGraph( TArray<UObject*>& Objects );

	void ClearSearchFlags();

	/** A complete graph of all references between all objects in memory */
	TMap<UObject*, FObjectGraphNode*> ObjectGraph;
};
