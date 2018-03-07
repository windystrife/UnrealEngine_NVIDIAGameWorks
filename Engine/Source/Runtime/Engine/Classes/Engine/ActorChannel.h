// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/NetworkGuid.h"
#include "Engine/Channel.h"
#include "Net/DataReplication.h"
#include "ActorChannel.generated.h"

class AActor;
class FInBunch;
class FNetFieldExportGroup;
class FOutBunch;
class UNetConnection;

/**
 * A channel for exchanging actor and its subobject's properties and RPCs.
 *
 * ActorChannel manages the creation and lifetime of a replicated actor. Actual replication of properties and RPCs
 * actually happens in FObjectReplicator now (see DataReplication.h).
 *
 * An ActorChannel bunch looks like this:
 *
 *		|----------------------|---------------------------------------------------------------------------|
 *		| SpawnInfo		       | (Spawn Info) Initial bunch only                                           |
 *		|  -Actor Class        |	-Created by ActorChannel	                                           |
 *		|  -Spawn Loc/Rot      |                                                                           |
 *      | NetGUID assigns      |                                                                           |
 *		|  -Actor NetGUID      |                                                                           |
 *		|  -Component NetGUIDs |                                                                           |
 *		|----------------------|---------------------------------------------------------------------------|
 *		|                      |                                                                           |
 *		|----------------------|---------------------------------------------------------------------------|
 *		| NetGUID ObjRef       | (Content chunks) x number of replicating objects (Actor + any components) |
 * 		|                      |		-Each chunk created by its own FObjectReplicator instance.         |
 * 		|----------------------|---------------------------------------------------------------------------|
 *      |                      |		                                                                   |
 *		| Properties...        |                                                                           |
 *		|                      |	                                                                       |
 *		| RPCs...              |                                                                           |
 *      |                      |                                                                           |
 *      |----------------------|---------------------------------------------------------------------------|
 *		| </End Tag>           |                                                                           |
 *		|----------------------|---------------------------------------------------------------------------|
 */
UCLASS(transient, customConstructor)
class ENGINE_API UActorChannel
	: public UChannel
{
	GENERATED_UCLASS_BODY()

	friend class FObjectReplicator;

	// Variables.
	UPROPERTY()
	AActor* Actor;					// Actor this corresponds to.

	FNetworkGUID	ActorNetGUID;		// Actor GUID (useful when we don't have the actor resolved yet). Currently only valid on clients.
	float			CustomTimeDilation;

	// Variables.
	double	RelevantTime;			// Last time this actor was relevant to client.
	double	LastUpdateTime;			// Last time this actor was replicated.
	uint32  SpawnAcked:1;			// Whether spawn has been acknowledged.
	uint32  bForceCompareProperties:1;	// Force this actor to compare all properties for a single frame
	uint32  bIsReplicatingActor:1;	// true when in this channel's ReplicateActor() to avoid recursion as that can cause invalid data to be sent
	
	/** whether we should nullptr references to this channel's Actor in other channels' Recent data when this channel is closed
	 * set to false in cases where the Actor can't become relevant again (e.g. destruction) as it's unnecessary in that case
	 */
	uint32 bClearRecentActorRefs:1;
	
	FObjectReplicator * ActorReplicator;

	TMap< TWeakObjectPtr< UObject >, TSharedRef< FObjectReplicator > > ReplicationMap;

	// Async networking loading support state
	TArray< class FInBunch * >			QueuedBunches;			// Queued bunches waiting on pending guids to resolve
	double								QueuedBunchStartTime;	// Time when since queued bunches was last empty
	TSet< FNetworkGUID >				PendingGuidResolves;	// These guids are waiting for their resolves, we need to queue up bunches until these are resolved

	TArray< TWeakObjectPtr< UObject > >	CreateSubObjects;		// Any sub-object we created on this channel

	TArray< FNetworkGUID >				QueuedMustBeMappedGuidsInLastBunch;		// Array of guids that will async load on client. This list is used for queued RPC's.

	TArray< class FOutBunch * >			QueuedExportBunches;			// Bunches that need to be appended to the export list on the next SendBunch call. This list is used for queued RPC's.

#if !UE_BUILD_SHIPPING
	/** Whether or not to block sending of NMT_ActorChannelFailure (for NetcodeUnitTest) */
	bool bBlockChannelFailure;
#endif

	/**
	 * Default constructor
	 */
	UActorChannel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: UChannel(ObjectInitializer)
#if !UE_BUILD_SHIPPING
		, bBlockChannelFailure(false)
#endif
	{
		ChType = CHTYPE_Actor;
		bClearRecentActorRefs = true;
	}

public:

	// UChannel interface.

	virtual void Init( UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally ) override;
	virtual void SetClosingFlag() override;
	virtual void ReceivedBunch( FInBunch& Bunch ) override;
	virtual void Tick() override;
	virtual bool CanStopTicking() const override;

	void ProcessBunch( FInBunch & Bunch );
	bool ProcessQueuedBunches();

	virtual void ReceivedNak( int32 NakPacketId ) override;
	virtual void Close() override;
	virtual FString Describe() override;

public:

	/** UActorChannel interface and accessors. */
	AActor* GetActor() {return Actor;}

	/** Replicate this channel's actor differences. */
	bool ReplicateActor();

	/** Allocate replication tables for the actor channel. */
	void SetChannelActor( AActor* InActor );

	virtual void NotifyActorChannelOpen(AActor* InActor, FInBunch& InBunch);

	void SetChannelActorForDestroy( struct FActorDestructionInfo *DestructInfo );

	/** Append any export bunches */
	virtual void AppendExportBunches( TArray< FOutBunch* >& OutExportBunches ) override;

	/** Append any "must be mapped" guids to front of bunch. These are guids that the client will wait on before processing this bunch. */
	virtual void AppendMustBeMappedGuids( FOutBunch* Bunch ) override;

	virtual void Serialize(FArchive& Ar) override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/** Queue a function bunch for this channel to be sent on the next property update. */
	void QueueRemoteFunctionBunch( UObject* CallTarget, UFunction* Func, FOutBunch &Bunch );

	/** Returns true if channel is ready to go dormant (e.g., all outstanding property updates have been ACK'd) */
	virtual bool ReadyForDormancy(bool debug=false) override;
	
	/** Puts the channel in a state to start becoming dormant. It will not become dormant until ReadyForDormancy returns true in Tick */
	virtual void StartBecomingDormant() override;

	/** Cleans up replicators and clears references to the actor class this channel was associated with.*/
	void CleanupReplicators( const bool bKeepReplicators = false );

	/** Writes the header for a content block of properties / RPCs for the given object (either the actor a subobject of the actor) */
	void WriteContentBlockHeader( UObject* Obj, FOutBunch &Bunch, const bool bHasRepLayout );

	/** Writes the header for a content block specifically for deleting sub-objects */
	void WriteContentBlockForSubObjectDelete( FOutBunch & Bunch, FNetworkGUID & GuidToDelete );

	/** Writes header and payload of content block */
	int32 WriteContentBlockPayload( UObject* Obj, FOutBunch &Bunch, const bool bHasRepLayout, FNetBitWriter& Payload );

	/** Reads the header of the content block and instantiates the subobject if necessary */
	UObject* ReadContentBlockHeader( FInBunch& Bunch, bool& bObjectDeleted, bool& bOutHasRepLayout );

	/** Reads content block header and payload */
	UObject* ReadContentBlockPayload( FInBunch &Bunch, FNetBitReader& OutPayload, bool& bOutHasRepLayout );

	/** Writes property/function header and data blob to network stream */
	int32 WriteFieldHeaderAndPayload( FNetBitWriter& Bunch, const FClassNetCache* ClassCache, const FFieldNetCache* FieldCache, FNetFieldExportGroup* NetFieldExportGroup, FNetBitWriter& Payload );

	/** Reads property/function header and data blob from network stream */
	bool ReadFieldHeaderAndPayload( UObject* Object, const FClassNetCache* ClassCache, FNetFieldExportGroup* NetFieldExportGroup, FNetBitReader& Bunch, const FFieldNetCache** OutField, FNetBitReader& OutPayload ) const;

	/** Finds the net field export group for a class net cache, if not found, creates one */
	FNetFieldExportGroup* GetNetFieldExportGroupForClassNetCache( const UClass* ObjectClass );
		
	/** Finds (or creates) the net field export group for a class net cache, if not found, creates one */
	FNetFieldExportGroup* GetOrCreateNetFieldExportGroupForClassNetCache( const UObject* Object );

	/** Returns the replicator for the actor associated with this channel. Guaranteed to exist. */
	FObjectReplicator & GetActorReplicationData();

	// --------------------------------
	// Subobject Replication state
	//
	//	Concepts: 
	//		ObjID  - this is an arbitrary identifier given to us by the game code.
	//		RepKey - this is an idenifier for the current replicated state. 
	//
	//	ObjID should be constant per object or "category". Its up to the game code. For example the game code could use 0 to determine if an entire array is dirty,
	//	then usen 1-N for each subobject in that list. Or it could have 5 arrays using 0-4, and then use 100*ArrayNum + idx for the items in the array.
	//
	//	RepKey should change as the subobject changes. Each time a subobject is marked dirty, its RepKey should change.
	//
	//	GameCode should call ::KeyNeedsToReplicate(ObjID, RepKey) to determine if it needs to replicate. For example:
	//
	//
	/*

	bool AMyActorClass::ReplicateSubobjects(UActorChannel *Channel, FOutBunch *Bunch, FReplicationFlags *RepFlags)
	{
		bool WroteSomething = false;

		if (Channel->KeyNeedsToReplicate(0, ReplicatedArrayKey) )	// Does the array need to replicate?
		{
			for (int32 idx = 0; idx < ReplicatedSubobjects.Num(); ++idx )
			{
				UMyActorSubobjClass *Obj = ReplicatedSubObjects[idx];
				if (Channel->KeyNeedsToReplicate(1 + idx, Obj->RepKey))
				{								
					WroteSomething |= Channel->ReplicateSubobject<UMyActorSubobjClass>(Obj, *Bunch, *RepFlags);
				}
			}
		}

		return WroteSomething;
	}

	void UMyActorSubobjClass::MarkDirtyForReplication()
	{
		this->RepKey++;
		MyOwningActor->ReplicatedArrayKey++;
	}

	*/
	//	
	// --------------------------------

	/** Replicates given subobject on this actor channel */
	bool ReplicateSubobject(UObject *Obj, FOutBunch &Bunch, const FReplicationFlags &RepFlags);

	/** utility template for replicating list of replicated subobjects */
	template<typename Type>
	bool ReplicateSubobjectList(TArray<Type*> &ObjectList, FOutBunch &Bunch, const FReplicationFlags &RepFlags)
	{
		bool WroteSomething = false;
		for (auto It = ObjectList.CreateIterator(); It; ++It)
		{
			Type* Obj = *It;
			WroteSomething |= ReplicateSubobject(Obj, Bunch, RepFlags);
		}

		return WroteSomething;
	}

	// Static size for SubobjectRepKeyMap. Allows us to resuse arrays and avoid dyanmic memory allocations
	static const int32 SubobjectRepKeyBufferSize = 64;

	struct FPacketRepKeyInfo
	{
		FPacketRepKeyInfo() : PacketID(INDEX_NONE) { }

		int32			PacketID;
		TArray<int32>	ObjKeys;
	};

	// Maps ObjID to the current RepKey
	TMap<int32, int32>		SubobjectRepKeyMap;
	
	// Maps packetId to keys in Subobject
	TMap<int32, FPacketRepKeyInfo >		SubobjectNakMap;

	// Keys pending in this bunch
	TArray<int32> PendingObjKeys;
	
	// Returns true if the given ObjID is not up to date with RepKey
	// this implicitly 'writes' the RepKey to the current out bunch.
	bool KeyNeedsToReplicate(int32 ObjID, int32 RepKey);
	
	// --------------------------------

protected:
	
	TSharedRef< FObjectReplicator > & FindOrCreateReplicator(UObject *Obj);
	bool ObjectHasReplicator(UObject *Obj);	// returns whether we have already created a replicator for this object or not

	/** Unmap all references to this object, so that if later we receive this object again, we can remap the original references */
	void MoveMappedObjectToUnmapped( const UObject* Object );

	void DestroyActorAndComponents();

	virtual bool CleanUp( const bool bForDestroy ) override;

	/** Closes the actor channel but with a 'dormant' flag set so it can be reopened */
	virtual void BecomeDormant() override;
};
