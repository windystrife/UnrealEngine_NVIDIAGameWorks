// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RepLayout.h:
	FRepLayout is a helper class to quickly replicate properties that are marked for replication
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/NetworkGuid.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "GCObject.h"

class FGuidReferences;
class FNetFieldExportGroup;
class FRepLayout;
class UActorChannel;
class UNetConnection;
class UPackageMapClient;

// Properties will be copied in here so memory needs aligned to largest type
typedef TArray< uint8, TAlignedHeapAllocator<16> > FRepStateStaticBuffer;


class FRepChangedParent
{
public:
	FRepChangedParent() : Active( 1 ), OldActive( 1 ), IsConditional( 0 ) {}

	uint32				Active			: 1;
	uint32				OldActive		: 1;
	uint32				IsConditional	: 1;
};

/** FRepChangedPropertyTracker
 * This class is used to store the change list for a group of properties of a particular actor/object
 * This information is shared across connections when possible
 */
class FRepChangedPropertyTracker : public IRepChangedPropertyTracker
{
public:
	FRepChangedPropertyTracker( const bool InbIsReplay, const bool InbIsClientReplayRecording )
		: bIsReplay( InbIsReplay )
		, bIsClientReplayRecording( InbIsClientReplayRecording )
		, ExternalDataNumBits( 0 ) {}
	virtual ~FRepChangedPropertyTracker() { }

	virtual void SetCustomIsActiveOverride( const uint16 RepIndex, const bool bIsActive ) override
	{
		FRepChangedParent & Parent = Parents[RepIndex];

		checkSlow( Parent.IsConditional );

		Parent.Active = (bIsActive || bIsClientReplayRecording) ? 1 : 0;

		Parent.OldActive = Parent.Active;
	}

	virtual void SetExternalData( const uint8* Src, const int32 NumBits ) override
	{
		ExternalDataNumBits = NumBits;
		const int32 NumBytes = ( NumBits + 7 ) >> 3;
		ExternalData.Reset( NumBytes );
		ExternalData.AddUninitialized( NumBytes );
		FMemory::Memcpy( ExternalData.GetData(), Src, NumBytes );
	}

	virtual bool IsReplay() const override
	{
		return bIsReplay;
	}

	TArray< FRepChangedParent >	Parents;

	bool						bIsReplay;							// True when recording/playing replays
	bool						bIsClientReplayRecording;			// True when recording client replays

	TArray< uint8 >				ExternalData;
	uint32						ExternalDataNumBits;
};

class FRepLayout;

class FRepChangedHistory
{
public:
	FRepChangedHistory() : Resend( false ) {}

	FPacketIdRange		OutPacketIdRange;
	TArray< uint16 >	Changed;
	bool				Resend;
};

class FGuidReferences;

typedef TMap< int32, FGuidReferences > FGuidReferencesMap;

class FGuidReferences
{
public:
	FGuidReferences() : NumBufferBits( 0 ), Array( NULL ) {}
	FGuidReferences( FBitReader& InReader, FBitReaderMark& InMark, const TSet< FNetworkGUID >& InUnmappedGUIDs, const TSet< FNetworkGUID >& InMappedDynamicGUIDs, const int32 InParentIndex, const int32 InCmdIndex ) : UnmappedGUIDs( InUnmappedGUIDs ), MappedDynamicGUIDs( InMappedDynamicGUIDs ), Array( NULL ), ParentIndex( InParentIndex ), CmdIndex( InCmdIndex )
	{
		NumBufferBits = InReader.GetPosBits() - InMark.GetPos();
		InMark.Copy( InReader, Buffer );
	}
	FGuidReferences( FGuidReferencesMap* InArray, const int32 InParentIndex, const int32 InCmdIndex ) : NumBufferBits( 0 ), Array( InArray ), ParentIndex( InParentIndex ), CmdIndex( InCmdIndex ) {}

	~FGuidReferences();

	TSet< FNetworkGUID >		UnmappedGUIDs;
	TSet< FNetworkGUID >		MappedDynamicGUIDs;
	TArray< uint8 >				Buffer;
	int32						NumBufferBits;

	FGuidReferencesMap*			Array;
	int32						ParentIndex;
	int32						CmdIndex;
};

/** FRepChangelistState
*  Stores changelist history (that are used to know what properties have changed) for objects
*/
class FRepChangelistState
{
public:
	FRepChangelistState() :
		HistoryStart( 0 ),
		HistoryEnd( 0 ),
		CompareIndex( 0 )
	{ }

	~FRepChangelistState();

	TSharedPtr< FRepLayout >						RepLayout;

	static const int32 MAX_CHANGE_HISTORY = 64;

	FRepChangedHistory								ChangeHistory[MAX_CHANGE_HISTORY];
	int32											HistoryStart;
	int32											HistoryEnd;
	int32											CompareIndex;

	FRepStateStaticBuffer							StaticBuffer;
};

/** FRepState
 *  Stores state used by the FRepLayout manager
*/
class FRepState
{
public:
	FRepState() : 
		HistoryStart( 0 ), 
		HistoryEnd( 0 ),
		NumNaks( 0 ),
		OpenAckedCalled( false ),
		AwakeFromDormancy( false ),
		LastChangelistIndex( 0 ),
		LastCompareIndex( 0 )
	{ }

	~FRepState();

	FRepStateStaticBuffer			StaticBuffer;

	FGuidReferencesMap				GuidReferencesMap;

	TSharedPtr< FRepLayout >		RepLayout;
	
	TArray< UProperty * >			RepNotifies;

	TSharedPtr<FRepChangedPropertyTracker> RepChangedPropertyTracker;

	static const int32 MAX_CHANGE_HISTORY = 32;

	FRepChangedHistory				ChangeHistory[MAX_CHANGE_HISTORY];
	int32							HistoryStart;
	int32							HistoryEnd;

	int32							NumNaks;

	TArray< FRepChangedHistory >	PreOpenAckHistory;

	bool							OpenAckedCalled;
	bool							AwakeFromDormancy;

	FReplicationFlags				RepFlags;

	TArray< uint16 >				LifetimeChangelist;			// This is the unique list of properties that have changed since the channel was first opened

	int32							LastChangelistIndex;		// The last change list history item we replicated from FRepChangelistState (if we are caught up to FRepChangelistState::HistoryEnd, there are no new changelists to replicate)
	int32							LastCompareIndex;			// If == FRepChangelistState::CompareIndex, then there is definitely no new information since the last time we checked

	bool							ConditionMap[COND_Max];
};

enum ERepLayoutCmdType
{
	REPCMD_DynamicArray			= 0,	// Dynamic array
	REPCMD_Return				= 1,	// Return from array, or end of stream
	REPCMD_Property				= 2,	// Generic property

	REPCMD_PropertyBool			= 3,
	REPCMD_PropertyFloat		= 4,
	REPCMD_PropertyInt			= 5,
	REPCMD_PropertyByte			= 6,
	REPCMD_PropertyName			= 7,
	REPCMD_PropertyObject		= 8,
	REPCMD_PropertyUInt32		= 9,
	REPCMD_PropertyVector		= 10,
	REPCMD_PropertyRotator		= 11,
	REPCMD_PropertyPlane		= 12,
	REPCMD_PropertyVector100	= 13,
	REPCMD_PropertyNetId		= 14,
	REPCMD_RepMovement			= 15,
	REPCMD_PropertyVectorNormal	= 16,
	REPCMD_PropertyVector10		= 17,
	REPCMD_PropertyVectorQ		= 18,
	REPCMD_PropertyString		= 19,
	REPCMD_PropertyUInt64		= 20,
};

enum ERepParentFlags
{
	PARENT_IsLifetime			= ( 1 << 0 ),
	PARENT_IsConditional		= ( 1 << 1 ),		// True if this property has a secondary condition to check
	PARENT_IsConfig				= ( 1 << 2 ),		// True if this property is defaulted from a config file
	PARENT_IsCustomDelta		= ( 1 << 3 )		// True if this property uses custom delta compression
};

class FRepParentCmd
{
public:
	FRepParentCmd( UProperty * InProperty, int32 InArrayIndex ) : 
		Property( InProperty ), 
		ArrayIndex( InArrayIndex ), 
		CmdStart( 0 ), 
		CmdEnd( 0 ), 
		RoleSwapIndex( -1 ), 
		Condition( COND_None ),
		RepNotifyCondition(REPNOTIFY_OnChanged),
		Flags( 0 )
	{}

	UProperty *			Property;
	int32				ArrayIndex;
	uint16				CmdStart;
	uint16				CmdEnd;
	int32				RoleSwapIndex;
	ELifetimeCondition	Condition;
	ELifetimeRepNotifyCondition	RepNotifyCondition;

	uint32				Flags;
};

class FRepLayoutCmd
{
public:
	UProperty * Property;			// Pointer back to property, used for NetSerialize calls, etc.
	uint8		Type;
	uint16		EndCmd;				// For arrays, this is the cmd index to jump to, to skip this arrays inner elements
	uint16		ElementSize;		// For arrays, element size of data
	int32		Offset;				// Absolute offset of property
	uint16		RelativeHandle;		// Handle relative to start of array, or top list
	uint16		ParentIndex;		// Index into Parents
	uint32		CompatibleChecksum;	// Used to determine if property is still compatible
};
	
/** FHandleToCmdIndex
 *  Converts a relative handle to the appropriate index into the Cmds array
 */
class FHandleToCmdIndex
{
public:
	FHandleToCmdIndex() : CmdIndex( INDEX_NONE )
	{
	}

	FHandleToCmdIndex( const int32 InHandleToCmdIndex ) : CmdIndex( InHandleToCmdIndex )
	{
	}

	int32										CmdIndex;
	TUniquePtr< TArray< FHandleToCmdIndex > >	HandleToCmdIndex;

	// Fix VS2013 compiler issue (VS2013 doesn't synthesize move constructor or move assignment operator version of these)
	FHandleToCmdIndex( FHandleToCmdIndex&& Other ) : CmdIndex( Other.CmdIndex ), HandleToCmdIndex( MoveTemp( Other.HandleToCmdIndex ) )
	{
	}

	FHandleToCmdIndex& operator=( FHandleToCmdIndex&& Other )
	{
		if ( this != &Other )
		{
			CmdIndex			= Other.CmdIndex;
			HandleToCmdIndex	= MoveTemp( Other.HandleToCmdIndex );
		}

		return *this;
	}
};

class FChangelistIterator
{
public:
	FChangelistIterator( const TArray< uint16 >& InChanged, const int32 InChangedIndex ) : Changed( InChanged ), ChangedIndex( InChangedIndex )
	{
	}

	const TArray< uint16 >& Changed;
	int32					ChangedIndex;
};

/** FRepHandleIterator
 *  Iterates over a changelist, taking each handle, and mapping to rep layout index, array index, etc
 */
class FRepHandleIterator
{
public:
	FRepHandleIterator(
		FChangelistIterator&				InChangelistIterator,
		const TArray< FRepLayoutCmd >&		InCmds,
		const TArray< FHandleToCmdIndex >&	InHandleToCmdIndex,
		const int32							InElementSize,
		const int32							InMaxArrayIndex,
		const int32							InMinCmdIndex,
		const int32							InMaxCmdIndex
	) :
		ChangelistIterator( InChangelistIterator ),
		Cmds( InCmds ),
		HandleToCmdIndex( InHandleToCmdIndex ),
		NumHandlesPerElement( HandleToCmdIndex.Num() ),
		ArrayElementSize( InElementSize ),
		MaxArrayIndex( InMaxArrayIndex ),
		MinCmdIndex( InMinCmdIndex ),
		MaxCmdIndex( InMaxCmdIndex )
	{
	}

	FChangelistIterator&				ChangelistIterator;

	const TArray< FRepLayoutCmd >&		Cmds;
	const TArray< FHandleToCmdIndex >&	HandleToCmdIndex;
	const int32							NumHandlesPerElement;
	const int32							ArrayElementSize;
	const int32							MaxArrayIndex;
	const int32							MinCmdIndex;
	const int32							MaxCmdIndex;

	int32								Handle;
	int32								CmdIndex;
	int32								ArrayIndex;
	int32								ArrayOffset;

	bool	NextHandle();
	bool	JumpOverArray();
	int32	PeekNextHandle() const;
};

/** FRepLayout
 *  This class holds all replicated properties for a parent property, and all its children
 *	Helpers functions exist to read/write and compare property state.
*/
class FRepLayout : public FGCObject
{
	friend class FRepState;
	friend class FRepChangelistState;
	friend class UPackageMapClient;

public:
	FRepLayout() : FirstNonCustomParent( 0 ), RoleIndex( -1 ), RemoteRoleIndex( -1 ), Owner( NULL ) {}

	void OpenAcked( FRepState * RepState ) const;

	void InitShadowData(
		TArray< uint8, TAlignedHeapAllocator<16> >&	ShadowData,
		UClass *									InObjectClass,
		uint8 *										Src ) const;

	void InitRepState( 
		FRepState *									RepState, 
		UClass *									InObjectClass, 
		uint8 *										Src, 
		TSharedPtr< FRepChangedPropertyTracker > &	InRepChangedPropertyTracker ) const;

	void InitChangedTracker( FRepChangedPropertyTracker * ChangedTracker ) const;

	bool ReplicateProperties(
		FRepState* RESTRICT				RepState,
		FRepChangelistState* RESTRICT	RepChangelistState,
		const uint8* RESTRICT			Data,
		UClass *						ObjectClass,
		UActorChannel *					OwningChannel,
		FNetBitWriter&					Writer,
		const FReplicationFlags &		RepFlags ) const;

	void SendProperties(
		FRepState*	RESTRICT		RepState,
		FRepChangedPropertyTracker* ChangedTracker,
		const uint8* RESTRICT		Data,
		UClass*						ObjectClass,
		FNetBitWriter&				Writer,
		TArray< uint16 >&			Changed ) const;

	ENGINE_API void InitFromObjectClass( UClass * InObjectClass );

	bool ReceiveProperties( UActorChannel* OwningChannel, UClass * InObjectClass, FRepState * RESTRICT RepState, void* RESTRICT Data, FNetBitReader & InBunch, bool & bOutHasUnmapped, const bool bEnableRepNotifies, bool& bOutGuidsChanged ) const;

	void GatherGuidReferences( FRepState* RepState, TSet< FNetworkGUID >& OutReferencedGuids, int32& OutTrackedGuidMemoryBytes ) const;

	bool MoveMappedObjectToUnmapped( FRepState* RepState, const FNetworkGUID& GUID ) const;

	void UpdateUnmappedObjects( FRepState *	RepState, UPackageMap * PackageMap, UObject* Object, bool & bOutSomeObjectsWereMapped, bool & bOutHasMoreUnmapped ) const;

	void CallRepNotifies( FRepState * RepState, UObject* Object ) const;
	void PostReplicate( FRepState * RepState, FPacketIdRange & PacketRange, bool bReliable ) const;
	void ReceivedNak( FRepState * RepState, int32 NakPacketId ) const;
	bool AllAcked( FRepState * RepState ) const;
	bool ReadyForDormancy( FRepState * RepState ) const;

	void ValidateWithChecksum( const void* RESTRICT Data, FArchive & Ar ) const;
	uint32 GenerateChecksum( const FRepState* RepState ) const;

	/** Clamp the changelist so that it conforms to the current size of either the array, or arrays within structs/arrays */
	void PruneChangeList( FRepState * RepState, const void* RESTRICT Data, const TArray< uint16 >& Changed, TArray< uint16 >& PrunedChanged ) const;

	void MergeChangeList( const uint8* RESTRICT Data, const TArray< uint16 >& Dirty1, const TArray< uint16 >& Dirty2, TArray< uint16 >& MergedDirty ) const;

	bool DiffProperties( TArray<UProperty*>* RepNotifies, void* RESTRICT Destination, const void* RESTRICT Source, const bool bSync ) const;

	void GetLifetimeCustomDeltaProperties(TArray< int32 > & OutCustom, TArray< ELifetimeCondition >	& OutConditions);

	// RPC support
	void InitFromFunction( UFunction * InFunction );
	void SendPropertiesForRPC( UObject* Object, UFunction * Function, UActorChannel * Channel, FNetBitWriter & Writer, void* Data ) const;
	void ReceivePropertiesForRPC( UObject* Object, UFunction * Function, UActorChannel * Channel, FNetBitReader & Reader, void* Data, TSet<FNetworkGUID>& UnmappedGuids) const;

	// Struct support
	void SerializePropertiesForStruct( UStruct * Struct, FArchive & Ar, UPackageMap	* Map, void* Data, bool & bHasUnmapped ) const;	
	void InitFromStruct( UStruct * InStruct );

	// Serializes all replicated properties of a UObject in or out of an archive (depending on what type of archive it is)
	ENGINE_API void SerializeObjectReplicatedProperties(UObject* Object, FArchive & Ar) const;

	UObject* GetOwner() const { return Owner; }

	void SendProperties_BackwardsCompatible(
		FRepState* RESTRICT			RepState,
		FRepChangedPropertyTracker* ChangedTracker,
		const uint8* RESTRICT		Data,
		UNetConnection*				Connection,
		FNetBitWriter&				Writer,
		TArray< uint16 >&			Changed ) const;

	bool ReceiveProperties_BackwardsCompatible(
		UNetConnection*				Connection,
		FRepState* RESTRICT			RepState,
		void* RESTRICT				Data,
		FNetBitReader&				InBunch,
		bool&						bOutHasUnmapped,
		const bool					bEnableRepNotifies,
		bool&						bOutGuidsChanged ) const;

	bool CompareProperties(
		FRepChangelistState* RESTRICT	RepState,
		const uint8* RESTRICT			Data,
		const FReplicationFlags&		RepFlags ) const;

	ENGINE_API virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void RebuildConditionalProperties( FRepState * RESTRICT	RepState, const FRepChangedPropertyTracker& ChangedTracker, const FReplicationFlags& RepFlags ) const;

	void UpdateChangelistHistory( FRepState * RepState, UClass * ObjectClass, const uint8* RESTRICT Data, UNetConnection* Connection, TArray< uint16 > * OutMerged ) const;

	void SendProperties_BackwardsCompatible_r(
		FRepState* RESTRICT					RepState,
		UPackageMapClient*					PackageMapClient,
		FNetFieldExportGroup*				NetFieldExportGroup,
		FRepChangedPropertyTracker*			ChangedTracker,
		FNetBitWriter&						Writer,
		const bool							bDoChecksum,
		FRepHandleIterator&					HandleIterator,
		const uint8* RESTRICT				SourceData ) const;

	void SendAllProperties_BackwardsCompatible_r(
		FNetBitWriter&						Writer,
		const bool							bDoChecksum,
		UPackageMapClient*					PackageMapClient,
		FNetFieldExportGroup*				NetFieldExportGroup,
		const int32							CmdStart,
		const int32							CmdEnd,
		const uint8*						SourceData ) const;

	void SendProperties_r(
		FRepState*	RESTRICT				RepState,
		FRepChangedPropertyTracker*			ChangedTracker,
		FNetBitWriter&						Writer,
		const bool							bDoChecksum,
		FRepHandleIterator&					HandleIterator,
		const uint8* RESTRICT				SourceData ) const;

	uint16 CompareProperties_r(
		const int32				CmdStart,
		const int32				CmdEnd,
		const uint8* RESTRICT	CompareData,
		const uint8* RESTRICT	Data,
		TArray< uint16 >&		Changed,
		uint16					Handle,
		const bool				bIsInitial,
		const bool				bForceFail ) const;

	void CompareProperties_Array_r(
		const uint8* RESTRICT	CompareData,
		const uint8* RESTRICT	Data,
		TArray< uint16 >&		Changed,
		const uint16			CmdIndex,
		const uint16			Handle,
		const bool				bIsInitial,
		const bool				bForceFail ) const;

	TSharedPtr< FNetFieldExportGroup > CreateNetfieldExportGroup() const;

	int32 FindCompatibleProperty( const int32 CmdStart, const int32 CmdEnd, const uint32 Checksum ) const;

	bool ReceiveProperties_BackwardsCompatible_r(
		FRepState * RESTRICT	RepState,
		FNetFieldExportGroup*	NetFieldExportGroup,
		FNetBitReader &			Reader,
		const int32				CmdStart,
		const int32				CmdEnd,
		uint8* RESTRICT			ShadowData,
		uint8* RESTRICT			OldData,
		uint8* RESTRICT			Data,
		FGuidReferencesMap*		GuidReferencesMap,
		bool&					bOutHasUnmapped,
		bool&					bOutGuidsChanged ) const;

	void GatherGuidReferences_r( FGuidReferencesMap* GuidReferencesMap, TSet< FNetworkGUID >& OutReferencedGuids, int32& OutTrackedGuidMemoryBytes ) const;

	bool MoveMappedObjectToUnmapped_r( FGuidReferencesMap* GuidReferencesMap, const FNetworkGUID& GUID ) const;

	void UpdateUnmappedObjects_r(
		FRepState*				RepState, 
		FGuidReferencesMap*		GuidReferencesMap,
		UObject*				OriginalObject,
		UPackageMap*			PackageMap, 
		uint8* RESTRICT			StoredData, 
		uint8* RESTRICT			Data, 
		const int32				MaxAbsOffset,
		bool&					bOutSomeObjectsWereMapped,
		bool&					bOutHasMoreUnmapped ) const;

	void ValidateWithChecksum_DynamicArray_r( const FRepLayoutCmd& Cmd, const int32 CmdIndex, const uint8* RESTRICT Data, FArchive & Ar ) const;
	void ValidateWithChecksum_r( const int32 CmdStart, const int32 CmdEnd, const uint8* RESTRICT Data, FArchive & Ar ) const;

	void SanityCheckChangeList_DynamicArray_r( 
		const int32				CmdIndex, 
		const uint8* RESTRICT	Data, 
		TArray< uint16 > &		Changed,
		int32 &					ChangedIndex ) const;

	uint16 SanityCheckChangeList_r( 
		const int32				CmdStart, 
		const int32				CmdEnd, 
		const uint8* RESTRICT	Data, 
		TArray< uint16 > &		Changed,
		int32 &					ChangedIndex,
		uint16					Handle ) const;

	void SanityCheckChangeList( const uint8* RESTRICT Data, TArray< uint16 > & Changed ) const;

	uint16 AddParentProperty( UProperty * Property, int32 ArrayIndex );

	int32 InitFromProperty_r( UProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex, uint32 ParentChecksum, int32 StaticArrayIndex );

	uint32 AddPropertyCmd( UProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex, uint32 ParentChecksum, int32 StaticArrayIndex );
	uint32 AddArrayCmd( UArrayProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex, uint32 ParentChecksum, int32 StaticArrayIndex );
	void AddReturnCmd();

	void SerializeProperties_DynamicArray_r( 
		FArchive &			Ar, 
		UPackageMap	*		Map,
		const int32			CmdIndex,
		uint8 *				Data,
		bool &				bHasUnmapped ) const;

	void SerializeProperties_r( 
		FArchive &			Ar, 
		UPackageMap	*		Map,
		const int32			CmdStart, 
		const int32			CmdEnd, 
		void *				Data,
		bool &				bHasUnmapped ) const;

	void MergeChangeList_r(
		FRepHandleIterator&		RepHandleIterator1,
		FRepHandleIterator&		RepHandleIterator2,
		const uint8* RESTRICT	SourceData,
		TArray< uint16 >&		OutChanged ) const;

	void PruneChangeList_r(
		FRepHandleIterator&		RepHandleIterator,
		const uint8* RESTRICT	SourceData,
		TArray< uint16 >&		OutChanged ) const;
		
	void BuildChangeList_r( const TArray< FHandleToCmdIndex >& HandleToCmdIndex, const int32 CmdStart, const int32 CmdEnd, uint8* Data, const int32 HandleOffset, TArray< uint16 >& Changed ) const;

	void BuildHandleToCmdIndexTable_r( const int32 CmdStart, const int32 CmdEnd, TArray< FHandleToCmdIndex >& HandleToCmdIndex );

	void ConstructProperties( TArray< uint8, TAlignedHeapAllocator<16> >& ShadowData ) const;
	void InitProperties( TArray< uint8, TAlignedHeapAllocator<16> >& ShadowData, uint8* Src ) const;
	void DestructProperties( FRepStateStaticBuffer& RepStateStaticBuffer ) const;

	TArray< FRepParentCmd >		Parents;
	TArray< FRepLayoutCmd >		Cmds;

	TArray< FHandleToCmdIndex >	BaseHandleToCmdIndex;		// Converts a relative handle to the appropriate index into the Cmds array

	int32						FirstNonCustomParent;
	int32						RoleIndex;
	int32						RemoteRoleIndex;

	UObject *					Owner;						// Either a UCkass or UFunction
};
