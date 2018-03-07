// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ScriptObjectReferencer.h"

FScriptObjectReferencer* FScriptObjectReferencer::Singleton = NULL;

void FScriptObjectReferencer::Init()
{
	check(Singleton == NULL);
	Singleton = new FScriptObjectReferencer();
}

void FScriptObjectReferencer::Shutdown()
{
	delete Singleton;
	Singleton = NULL;
}

FScriptObjectReferencer& FScriptObjectReferencer::Get()
{
	check(Singleton);
	return *Singleton;
}

void FScriptObjectReferencer::AddObjectReference(UObject* Object)
{
	ScriptCreatedObjects.Add(Object);
}

void FScriptObjectReferencer::RemoveObjectReference(UObject* Object)
{
	ScriptCreatedObjects.Remove(Object);
}

void FScriptObjectReferencer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AllowEliminatingReferences(false);
	for (auto Object : ScriptCreatedObjects)
	{
		Collector.AddReferencedObject(Object);
	}
	Collector.AllowEliminatingReferences(true);
}