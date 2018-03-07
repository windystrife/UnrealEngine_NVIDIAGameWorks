// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"


/**
 * This class provides common registration for gamethread editor only tickable objects. It is an
 * abstract base class requiring you to implement the GetStatId, IsTickable, and Tick methods.
 */
class FTickableEditorObject : public FTickableObjectBase
{
public:

	static void TickObjects(float DeltaSeconds)
	{
		const TArray<FTickableEditorObject*>& TickableObjects = GetTickableObjects();

		for (int32 ObjectIndex=0; ObjectIndex < TickableObjects.Num(); ++ObjectIndex)
		{
			FTickableEditorObject* TickableObject = TickableObjects[ObjectIndex];
			if (TickableObject->IsTickable())
			{
				TickableObject->Tick(DeltaSeconds);
			}
		}
	}

	/** Registers this instance with the static array of tickable objects. */
	FTickableEditorObject()
	{
		GetTickableObjects().Add(this);
	}

	/** Removes this instance from the static array of tickable objects. */
	virtual ~FTickableEditorObject()
	{
		UnregisterTickableObject(this);
	}

private:

	/**
	 * Class that avoids crashes when unregistering a tickable editor object too late.
	 *
	 * Some tickable objects can outlive the collection
	 * (global/static destructor order is unpredictable).
	 */
	class TTickableObjectsCollection : public TArray<FTickableEditorObject*>
	{
	public:
		~TTickableObjectsCollection()
		{
			FTickableEditorObject::bCollectionIntact = false;
		}
	};

	friend class TTickableObjectsCollection;

	/** True if collection of tickable objects is still intact. */
	UNREALED_API static bool bCollectionIntact;

	/** Avoids removal if the object outlived the collection. */
	UNREALED_API static void UnregisterTickableObject(FTickableEditorObject* Obj)
	{
		if (bCollectionIntact)
		{
			const int32 Pos = GetTickableObjects().Find(Obj);
			check(Pos!=INDEX_NONE);
			GetTickableObjects().RemoveAt(Pos);
		}
	}

	/** Returns the array of tickable editor objects */
	UNREALED_API static TArray<FTickableEditorObject*>& GetTickableObjects()
	{
		static TTickableObjectsCollection TickableObjects;
		return TickableObjects;
	}
};
