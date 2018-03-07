// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetConnection.h"

class AActor;

/**
 * Struct to store an actor pointer and any internal metadata for that actor used
 * internally by a UNetDriver.
 */
struct FNetworkObjectInfo
{
	/** Pointer to the replicated actor. */
	AActor* Actor;

	/** Next time to consider replicating the actor. Based on FPlatformTime::Seconds(). */
	double NextUpdateTime;

	/** Last absolute time in seconds since actor actually sent something during replication */
	double LastNetReplicateTime;

	/** Optimal delta between replication updates based on how frequently actor properties are actually changing */
	float OptimalNetUpdateDelta;

	/** Last time this actor was updated for replication via NextUpdateTime
	* @warning: internal net driver time, not related to WorldSettings.TimeSeconds */
	float LastNetUpdateTime;

	/** Is this object still pending a full net update due to clients that weren't able to replicate the actor at the time of LastNetUpdateTime */
	uint32 bPendingNetUpdate : 1;

	/** Force this object to be considered relevant for at least one update */
	uint32 bForceRelevantNextUpdate : 1;

	/** List of connections that this actor is dormant on */
	TSet<TWeakObjectPtr<UNetConnection>> DormantConnections;

	/** A list of connections that this actor has recently been dormant on, but the actor doesn't have a channel open yet.
	*  These need to be differentiated from actors that the client doesn't know about, but there's no explicit list for just those actors.
	*  (this list will be very transient, with connections being moved off the DormantConnections list, onto this list, and then off once the actor has a channel again)
	*/
	TSet<TWeakObjectPtr<UNetConnection>> RecentlyDormantConnections;

	FNetworkObjectInfo()
		: Actor(nullptr)
		, NextUpdateTime(0.0)
		, LastNetReplicateTime(0.0)
		, OptimalNetUpdateDelta(0.0f)
		, LastNetUpdateTime(0.0f)
		, bPendingNetUpdate(false)
		, bForceRelevantNextUpdate(false) {}

	FNetworkObjectInfo(AActor* InActor)
		: Actor(InActor)
		, NextUpdateTime(0.0)
		, LastNetReplicateTime(0.0)
		, OptimalNetUpdateDelta(0.0f) 
		, LastNetUpdateTime(0.0f)
		, bPendingNetUpdate(false)
		, bForceRelevantNextUpdate(false) {}
};

/**
 * KeyFuncs to allow using the actor pointer as the comparison key in a set.
 */
struct FNetworkObjectKeyFuncs : BaseKeyFuncs<TSharedPtr<FNetworkObjectInfo>, AActor*, false>
{
	/**
	 * @return The key used to index the given element.
	 */
	static KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Get()->Actor;
	}

	/**
	 * @return True if the keys match.
	 */
	static bool Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}

	/** Calculates a hash index for a key. */
	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

/**
 * Stores the list of replicated actors for a given UNetDriver.
 */
class FNetworkObjectList
{
public:
	typedef TSet<TSharedPtr<FNetworkObjectInfo>, FNetworkObjectKeyFuncs> FNetworkObjectSet;

	/**
	 * Adds replicated actors in World to the internal set of replicated actors.
	 * Used when a net driver is initialized after some actors may have already
	 * been added to the world.
	 *
	 * @param World The world from which actors are added.
	 * @param NetDriverName The name of the net driver to which this object list belongs.
	 */
	void AddInitialObjects(UWorld* const World, const FName NetDriverName);

	/** Adds Actor to the internal list if its NetDriverName matches, or if we're adding to the demo net driver. */
	TSharedPtr<FNetworkObjectInfo>* Add(AActor* const Actor, const FName NetDriverName);

	/** Removes actor from the internal list, and any cleanup that is necessary (i.e. resetting dormancy state) */
	void Remove(AActor* const Actor);

	/** Marks this object as dormant for the passed in connection */
	void MarkDormant(AActor* const Actor, UNetConnection* const Connection, const int32 NumConnections, const FName NetDriverName);

	/** Marks this object as active for the passed in connection */
	bool MarkActive(AActor* const Actor, UNetConnection* const Connection, const FName NetDriverName);

	/** Removes the recently dormant status from the passed in connection */
	void ClearRecentlyDormantConnection(AActor* const Actor, UNetConnection* const Connection, const FName NetDriverName);

	/** 
	 *	Does the necessary house keeping when a new connection is added 
	 *	When a new connection is added, we must add all objects back to the active list so the new connection will process it
	 *	Once the objects is dormant on that connection, it will then be removed from the active list again
	*/
	void HandleConnectionAdded();

	/** Clears all state related to dormancy */
	void ResetDormancyState();

	/** Returns a const reference to the entire set of tracked actors. */
	const FNetworkObjectSet& GetAllObjects() const { return AllNetworkObjects; }

	/** Returns a const reference to the active set of tracked actors. */
	const FNetworkObjectSet& GetActiveObjects() const { return ActiveNetworkObjects; }

	int32 GetNumDormantActorsForConnection( UNetConnection* const Connection ) const;

	/** Force this actor to be relevant for at least one update */
	void ForceActorRelevantNextUpdate(AActor* const Actor, const FName NetDriverName);
		
	void Reset();

private:
	FNetworkObjectSet AllNetworkObjects;
	FNetworkObjectSet ActiveNetworkObjects;
	FNetworkObjectSet ObjectsDormantOnAllConnections;

	TMap<TWeakObjectPtr<UNetConnection>, int32 > NumDormantObjectsPerConnection;
};
