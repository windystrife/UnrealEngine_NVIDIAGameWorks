// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

//////////////////////////////////////////////////////////////////////////
// FEditorObjectTracker

class FEditorObjectTracker : public FGCObject
{
public:
	FEditorObjectTracker(bool bInAllowOnePerClass = true)
		: bAllowOnePerClass(bInAllowOnePerClass)
	{}

	// FGCObject interface
	void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// End of FGCObject interface

	/** Returns an existing editor object for the specified class or creates one
	    if none exist */
	UObject* GetEditorObjectForClass( UClass* EdClass );

	void SetAllowOnePerClass(bool bInAllowOnePerClass)
	{
		bAllowOnePerClass = bInAllowOnePerClass;
	}
private:

	/** If true, it uses TMap, otherwise, it just uses TArray */
	bool bAllowOnePerClass;

	/** Tracks editor objects created for details panel */
	TMap< UClass*, UObject* >	EditorObjMap;

	/** Tracks editor objects created for detail panel */
	TArray<UObject*> EditorObjectArray;
};
