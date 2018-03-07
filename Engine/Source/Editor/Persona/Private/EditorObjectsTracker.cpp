// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditorObjectsTracker.h"
#include "UObject/Package.h"

void FEditorObjectTracker::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObjects(EditorObjMap);
	Collector.AddReferencedObjects(EditorObjectArray);
}

UObject* FEditorObjectTracker::GetEditorObjectForClass( UClass* EdClass )
{
	UObject *Obj = (bAllowOnePerClass && EditorObjMap.Contains(EdClass) ? *EditorObjMap.Find(EdClass) : nullptr);

	if (Obj == NULL)
	{
		FString ObjName = MakeUniqueObjectName(GetTransientPackage(), EdClass).ToString();
		ObjName += "_EdObj";
		Obj = NewObject<UObject>(GetTransientPackage(), EdClass, FName(*ObjName), RF_Public | RF_Standalone | RF_Transient);
		if (bAllowOnePerClass)
		{
			EditorObjMap.Add(EdClass, Obj);
		}
		else
		{
			EditorObjectArray.Add(Obj);
		}
	}
	return Obj;
}
