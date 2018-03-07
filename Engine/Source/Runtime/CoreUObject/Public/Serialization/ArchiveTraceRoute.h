// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/ArchiveObjectGraph.h"

/**
 * Archive for finding shortest path from root to a particular object.
 * Depth-first search.
 */
class FArchiveTraceRoute : public FArchiveUObject
{
	/**
	 * Simple container struct for grouping two FObjectGraphNodes together.
	 */
	struct FRouteLink
	{
		/**
		 * Constructor
		 */
		FRouteLink( FObjectGraphNode* InParent=NULL, FObjectGraphNode* InChild=NULL )
		: LinkParent(InParent), LinkChild(InChild)
		{}

		/**
		 * The node corresponding to the "referencing" object.
		 */
		FObjectGraphNode* LinkParent;

		/**
		 * The node corresponding to the "referenced" object.
		 */
		FObjectGraphNode* LinkChild;
	};

public:
	static COREUOBJECT_API TMap<UObject*,UProperty*> FindShortestRootPath( UObject* Object, bool bIncludeTransients, EObjectFlags KeepFlags );

	/**
	 * Retuns path to root created by e.g. FindShortestRootPath via a string.
	 *
	 * @param TargetObject	object marking one end of the route
	 * @param Route			route to print to log.
	 * @param String of root path
	 */
	static COREUOBJECT_API FString PrintRootPath( const TMap<UObject*,UProperty*>& Route, const UObject* TargetObject );

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FArchiveTraceRoute"); }

	/**
	 * Serializes the objects in the specified set; any objects encountered during serialization
	 * of an object are added to the object set and processed until no new objects are added.
	 *
	 * @param	Objects		the original set of objects to serialize; the original set will be preserved.
	 */
	void GenerateObjectGraph( TSparseArray<UObject*>& Objects );

	/**
	 * Recursively iterates over the referencing objects for the specified node, marking each with
	 * the current Depth value.  Stops once it reaches a route root.
	 *
	 * @param	ObjectNode	the node to evaluate.
	 */
	void CalculateReferenceDepthsForNode( FObjectGraphNode* ObjectNode );

	/**
	 * Searches through the objects referenced by CurrentNode for a record with a Depth lower than LowestDepth.
	 *
	 * @param	CurrentNode		the node containing the list of referenced objects that will be searched.
	 * @param	LowestDepth		the current number of links we are from the target object.
	 * @param	ClosestLink		if a trace route record is found with a lower depth value than LowestDepth, the link is saved to this value.
	 *
	 * @return	true if a closer link was discovered; false if no links were closer than lowest depth, or if we've reached the target object.
	 */
	bool FindClosestLink( FObjectGraphNode* CurrentNode, int32& LowestDepth, FRouteLink& ClosestLink );

private:
	FArchiveTraceRoute( UObject* TargetObject, TMap<UObject*,FTraceRouteRecord>& InRoutes, bool bShouldIncludeTransients, EObjectFlags KeepFlags );
	~FArchiveTraceRoute();

	/** Handles serialization of UObject references */
	FArchive& operator<<( class UObject*& Obj );

	/** A complete graph of all references between all objects in memory */
	TMap<UObject*, FObjectGraphNode*> ObjectGraph;

	/**
	 * The object currently being serialized; used by the overloaded serialization operator to determine the referencing object.
	 */
	UObject* CurrentReferencer;

	/** The set of objects encountered while serializing CurrentReferencer */
	TArray<UObject*> ObjectsToSerialize;

	/** the current number of object reference links away from the target object */
	int32 Depth;

	/** true if we should serialize objects marked RF_Transient */
	bool bIncludeTransients;

	/**
	 * A bitmask of object flags which indicates which objects should be included in the group of initial objects to be serialized;
	 * RF_RootSet will be automatically added to this bitmask, and OBJECTMARK_TagExp will automatically be removed.
	 */
	EObjectFlags	RequiredFlags;

};
