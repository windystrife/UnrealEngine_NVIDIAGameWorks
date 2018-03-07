// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Tickable.h: Interface for tickable objects.

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

/**
 * Base class for tickable objects
 */
class FTickableObjectBase
{
public:
	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within LevelTick.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( float DeltaTime ) = 0;

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	true if class is ready to be ticked, false otherwise.
	 */
	virtual bool IsTickable() const = 0;

	/** return the stat id to use for this tickable **/
	virtual TStatId GetStatId() const = 0;
};

/**
 * This class provides common registration for gamethread tickable objects. It is an
 * abstract base class requiring you to implement the Tick() method.
 */
class ENGINE_API FTickableGameObject : public FTickableObjectBase
{
	/** Static array of tickable objects */
	static TArray<FTickableGameObject*> TickableObjects;
	static bool bIsTickingObjects;

public:
	/**
	 * Registers this instance with the static array of tickable objects.	
	 *
	 */
	FTickableGameObject()
	{
		check(!TickableObjects.Contains(this));
		TickableObjects.Add( this );
	}

	/**
	 * Removes this instance from the static array of tickable objects.
	 */
	virtual ~FTickableGameObject()
	{
		// make sure this tickable object was registered from the game thread
		const int32 Pos = TickableObjects.Find(this);
		check(Pos!=INDEX_NONE);
		if (bIsTickingObjects)
		{
			TickableObjects[Pos] = nullptr;
		}
		else
		{
			TickableObjects.RemoveAt(Pos);
		}
	}

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 * Defaults to false, as that mimics old behavior.
	 *
	 * @return true if it should be ticked when paused, false otherwise
	 */
	virtual bool IsTickableWhenPaused() const
	{
		return false;
	}

	/**
	 * Used to determine whether the object should be ticked in the editor.  Defaults to false since
	 * that is the previous behavior.
	 *
	 * @return	true if this tickable object can be ticked in the editor
	 */
	virtual bool IsTickableInEditor() const
	{
		return false;
	}

	virtual UWorld* GetTickableGameObjectWorld() const 
	{ 
		return nullptr;
	}

	static void TickObjects(UWorld* World, int32 TickType, bool bIsPaused, float DeltaSeconds);
};
