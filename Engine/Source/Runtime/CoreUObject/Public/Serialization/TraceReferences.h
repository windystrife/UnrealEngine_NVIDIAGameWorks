// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Serialization/ArchiveObjectGraph.h"

// This traces referenced/referencer of an object using FArchiveObjectGraph 
class FTraceReferences
{
	FArchiveObjectGraph ArchiveObjectGraph;

	// internal recursive function for referencers/referenced
	void GetReferencerInternal( UObject* CurrentObject, TArray<FObjectGraphNode*> &OutReferencer, int32 CurrentDepth, int32 TargetDepth );
	void GetReferencedInternal( UObject* CurrentObject, TArray<FObjectGraphNode*> &OutReferenced, int32 CurrentDepth, int32 TargetDepth );

public:
	FTraceReferences( bool bIncludeTransients = false, EObjectFlags KeepFlags = RF_AllFlags );

	// returns referencer string of an object
	FString GetReferencerString( UObject* Object, int32 Depth = 100 ); 
	// returns referenced string of an object	
	FString GetReferencedString( UObject* Object, int32 Depth = 100 );

	// returns referencer object list of an object	
	int32 GetReferencer( UObject* Object, TArray<FObjectGraphNode*> &Referencer, bool bExcludeSelf=true, int32 Depth = 100 );
	// returns referenced object list of an object		
	int32 GetReferenced( UObject* Object, TArray<FObjectGraphNode*> &Referenced, bool bExcludeSelf=true, int32 Depth = 100 );
};
