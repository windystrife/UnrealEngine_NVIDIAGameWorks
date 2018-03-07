// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataReplication.h:
	Holds classes for data replication (properties and RPCs).
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/NetworkGuid.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "UObject/UnrealType.h"

class FNetFieldExportGroup;
class FOutBunch;
class FRepChangelistState;
class FRepLayout;
class FRepState;
class UNetConnection;
class UNetDriver;

bool FORCEINLINE IsCustomDeltaProperty( const UProperty* Property )
{
	const UStructProperty * StructProperty = Cast< UStructProperty >( Property );

	if ( StructProperty != NULL && StructProperty->Struct->StructFlags & STRUCT_NetDeltaSerializeNative )
	{
		return true;
	}

	return false;
}

/** struct containing property and offset for replicated actor properties */
struct FReplicatedActorProperty
{
	/** offset into the Actor where this reference is located - includes offsets from any outer structs */
	int32 Offset;
	/** Reference to property object */
	const class UObjectPropertyBase* Property;

	FReplicatedActorProperty(int32 InOffset, const UObjectPropertyBase* InProperty)
		: Offset(InOffset), Property(InProperty)
	{}
};

/** 
 *	FReplicationChangelistMgr manages a list of change lists for a particular replicated object that have occurred since the object started replicating
 *	Once the history is completely full, the very first changelist will then be merged with the next one (freeing a slot)
 *		This way we always have the entire history for join in progress players
 *	This information is then used by all connections, to share the compare work needed to determine what to send each connection
 *	Connections will send any changelist that is new since the last time the connection checked
 */
class ENGINE_API FReplicationChangelistMgr
{
public:
	FReplicationChangelistMgr( UNetDriver* InDriver, UObject* InObject );

	~FReplicationChangelistMgr();

	void Update( const UObject* InObject, const uint32 ReplicationFrame, const int32 LastCompareIndex, const FReplicationFlags& RepFlags, const bool bForceCompare );

	FRepChangelistState* GetRepChangelistState() const { return RepChangelistState.Get(); }

private:
	UNetDriver*							Driver;
	TSharedPtr< FRepLayout >			RepLayout;
	TUniquePtr< FRepChangelistState >	RepChangelistState;
	uint32								LastReplicationFrame;
};

/** FObjectReplicator
 *   Generic class that replicates properties for an object.
 *   All delta/diffing work is done in this class. 
 *	 Its primary job is to produce and consume chunks of properties/RPCs:
 *
 *		|----------------|
 *		| NetGUID ObjRef |
 * 		|----------------|
 *      |                |		
 *		| Properties...  |
 *		|                |	
 *		| RPCs...        |
 *      |                |
 *      |----------------|
 *		| </End Tag>     |
 *		|----------------|
 *	
 */
class ENGINE_API FObjectReplicator
{
public:
	FObjectReplicator() : 
		ObjectClass( NULL ), 
		bLastUpdateEmpty( false ), 
		bOpenAckCalled( false ),
		bForceUpdateUnmapped( false ),
		Connection( NULL ),
		OwningChannel( NULL ),
		RepState( NULL ),
		RemoteFunctions( NULL )
	{ }

	~FObjectReplicator() 
	{
		CleanUp();
	}

	UClass *										ObjectClass;
	FNetworkGUID									ObjectNetGUID;
	TWeakObjectPtr< UObject >						ObjectPtr;

	TArray<FPropertyRetirement>						Retirement;					// Property retransmission.
	TMap<int32, TSharedPtr<INetDeltaBaseState> >	RecentCustomDeltaState;		// This is the delta state we need to compare with when determining what to send to a client for custom delta properties
	TMap<int32, TSharedPtr<INetDeltaBaseState> >	CDOCustomDeltaState;		// Same as RecentCustomDeltaState, but this will always remain as the initial CDO version. We use this to send all properties since channel was first opened (for bResendAllDataSinceOpen)

	TArray< int32 >									LifetimeCustomDeltaProperties;
	TArray< ELifetimeCondition >					LifetimeCustomDeltaPropertyConditions;

	uint32											bLastUpdateEmpty	: 1;	// True if last update (ReplicateActor) produced no replicated properties
	uint32											bOpenAckCalled		: 1;
	uint32											bForceUpdateUnmapped: 1;	// True if we need to do an unmapped check next frame

	UNetConnection *								Connection;					// Connection this replicator was created on
	class UActorChannel	*							OwningChannel;

	TMap< int32, UStructProperty* >					UnmappedCustomProperties;

	TArray< UProperty*,TInlineAllocator< 32 > >		RepNotifies;
	TMap< UProperty*, TArray<uint8> >				RepNotifyMetaData;

	TSharedPtr< FRepLayout >						RepLayout;
	FRepState *										RepState;

	TSet< FNetworkGUID >							ReferencedGuids;
	int32											TrackedGuidMemoryBytes;

	TSharedPtr< FReplicationChangelistMgr >			ChangelistMgr;

	struct FRPCCallInfo 
	{
		FName	FuncName;
		int32	Calls;
		float	LastCallTime;
	};

	TArray< FRPCCallInfo >							RemoteFuncInfo;				// Meta information on pending net RPCs (to be sent)
	FOutBunch *										RemoteFunctions;

	struct FRPCPendingLocalCall
	{
		/** Index to the RPC that was delayed */
		int32 RPCFieldIndex;

		/** Flags this was replicated with */
		FReplicationFlags RepFlags;

		/** Buffer to serialize RPC out of */
		TArray<uint8> Buffer;

		/** Number of bits in buffer */
		int64 NumBits;

		/** Guids being waited on */
		TSet<FNetworkGUID> UnmappedGuids;

		FRPCPendingLocalCall(const FFieldNetCache* InRPCField, const FReplicationFlags& InRepFlags, FNetBitReader& InReader, const TSet<FNetworkGUID>& InUnmappedGuids)
			: RPCFieldIndex(InRPCField->FieldNetIndex), RepFlags(InRepFlags), Buffer(InReader.GetBuffer()), NumBits(InReader.GetNumBits()), UnmappedGuids(InUnmappedGuids)
		{}
	};

	TArray< FRPCPendingLocalCall >					PendingLocalRPCs;			// Information on RPCs that have been received but not yet executed

	void InitWithObject( UObject* InObject, UNetConnection * InConnection, bool bUseDefaultState = true );
	void CleanUp();

	void StartReplicating( class UActorChannel * InActorChannel );
	void StopReplicating( class UActorChannel * InActorChannel );

	/** Recent/dirty related functions */
	void InitRecentProperties( uint8* Source );

	/** Takes Data, and compares against shadow state to log differences */
	bool ValidateAgainstState( const UObject* ObjectState );

	static bool SerializeCustomDeltaProperty( UNetConnection * Connection, void* Src, UProperty * Property, uint32 ArrayIndex, FNetBitWriter & OutBunch, TSharedPtr<INetDeltaBaseState> & NewFullState, TSharedPtr<INetDeltaBaseState> & OldState );

	/** Packet was dropped */
	void	ReceivedNak( int32 NakPacketId );

	void	Serialize(FArchive& Ar);

	/** Writes dirty properties to bunch */
	void	ReplicateCustomDeltaProperties( FNetBitWriter & Bunch, FReplicationFlags RepFlags );
	bool	ReplicateProperties( FOutBunch & Bunch, FReplicationFlags RepFlags );
	void	PostSendBunch(FPacketIdRange & PacketRange, uint8 bReliable);
	
	bool	ReceivedBunch( FNetBitReader& Bunch, const FReplicationFlags& RepFlags, const bool bHasRepLayout, bool& bOutHasUnmapped );
	bool	ReceivedRPC(FNetBitReader& Reader, const FReplicationFlags& RepFlags, const FFieldNetCache* FieldCache, const bool bCanDelayRPC, bool& bOutDelayRPC, TSet<FNetworkGUID>& OutUnmappedGuids);
	void	UpdateGuidToReplicatorMap();
	bool	MoveMappedObjectToUnmapped( const FNetworkGUID& GUID );
	void	PostReceivedBunch();

	void	ForceRefreshUnreliableProperties();

	bool bHasReplicatedProperties;

	void QueueRemoteFunctionBunch( UFunction* Func, FOutBunch &Bunch );

	bool ReadyForDormancy(bool debug=false);

	void StartBecomingDormant();

	void CallRepNotifies(bool bSkipIfChannelHasQueuedBunches);

	void UpdateUnmappedObjects( bool & bOutHasMoreUnmapped );

	FORCEINLINE UObject *	GetObject() const { return ObjectPtr.Get(); }
	FORCEINLINE void		SetObject( UObject* NewObj ) { ObjectPtr = TWeakObjectPtr<UObject>( NewObj ); }

	FORCEINLINE void PreNetReceive()		
	{ 
		UObject* Object = GetObject();
		if ( Object != NULL )
		{
			Object->PreNetReceive(); 
		}
	}

	FORCEINLINE void PostNetReceive()	
	{ 
		UObject* Object = GetObject();
		if ( Object != NULL )
		{
			Object->PostNetReceive(); 
		}
	}

	void QueuePropertyRepNotify( UObject* Object, UProperty * Property, const int32 ElementIndex, TArray< uint8 > & MetaData );

	void WritePropertyHeaderAndPayload(
		UObject*				Object,
		UProperty*				Property,
		FNetFieldExportGroup*	NetFieldExportGroup,
		FNetBitWriter&			Bunch,
		FNetBitWriter&			Payload ) const;
};
