// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RepLayout.cpp: Unreal replication layout implementation.
=============================================================================*/

#include "Net/RepLayout.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "EngineStats.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetConnection.h"
#include "Net/NetworkProfiler.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetworkSettings.h"
#include "HAL/LowLevelMemTracker.h"

static TAutoConsoleVariable<int32> CVarDoPropertyChecksum( TEXT( "net.DoPropertyChecksum" ), 0, TEXT( "" ) );

FAutoConsoleVariable CVarDoReplicationContextString( TEXT( "net.ContextDebug" ), 0, TEXT( "" ) );

int32 LogSkippedRepNotifies = 0;
static FAutoConsoleVariable CVarLogSkippedRepNotifies(TEXT("Net.LogSkippedRepNotifies"), LogSkippedRepNotifies, TEXT("Log when the networking code skips calling a repnotify clientside due to the property value not changing."), ECVF_Default );

int32 MaxRepArraySize = UNetworkSettings::DefaultMaxRepArraySize;
int32 MaxRepArrayMemory = UNetworkSettings::DefaultMaxRepArrayMemory;

FConsoleVariableSinkHandle CreateMaxArraySizeCVarAndRegisterSink()
{
	static FAutoConsoleVariable CVarMaxArraySize(TEXT("net.MaxRepArraySize"), MaxRepArraySize, TEXT("Maximum allowable size for replicated dynamic arrays (in number of elements). Value must be between 1 and 65535."));
	static FConsoleCommandDelegate Delegate = FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			const int32 NewMaxRepArraySizeValue = CVarMaxArraySize->GetInt();

			if ((int32)UINT16_MAX < NewMaxRepArraySizeValue || 1 > NewMaxRepArraySizeValue)
			{
				UE_LOG(LogRepTraffic, Error,
					TEXT("SerializeProperties_DynamicArray_r: MaxRepArraySize (%l) must be between 1 and 65535. Cannot accept new value."),
					NewMaxRepArraySizeValue);

				// Use SetByConsole to guarantee the value gets updated.
				CVarMaxArraySize->Set(MaxRepArraySize, ECVF_SetByConsole);
			}
			else
			{
				MaxRepArraySize = NewMaxRepArraySizeValue;
			}
		}
	);

	return IConsoleManager::Get().RegisterConsoleVariableSink_Handle(Delegate);
}

FConsoleVariableSinkHandle CreateMaxArrayMemoryCVarAndRegisterSink()
{
	static FAutoConsoleVariableRef CVarMaxArrayMemory(TEXT("net.MaxRepArrayMemory"), MaxRepArrayMemory, TEXT("Maximum allowable size for replicated dynamic arrays (in bytes). Value must be between 1 and 65535"));
	static FConsoleCommandDelegate Delegate = FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			const int32 NewMaxRepArrayMemoryValue = CVarMaxArrayMemory->GetInt();

			if ((int32)UINT16_MAX < NewMaxRepArrayMemoryValue || 1 > NewMaxRepArrayMemoryValue)
			{
				UE_LOG(LogRepTraffic, Error,
					TEXT("SerializeProperties_DynamicArray_r: MaxRepArrayMemory (%l) must be between 1 and 65535. Cannot accept new value."),
					NewMaxRepArrayMemoryValue);

				// Use SetByConsole to guarantee the value gets updated.
				CVarMaxArrayMemory->Set(MaxRepArrayMemory, ECVF_SetByConsole);
			}
			else
			{
				MaxRepArrayMemory = NewMaxRepArrayMemoryValue;
			}
		}
	);

	return IConsoleManager::Get().RegisterConsoleVariableSink_Handle(Delegate);
}

// This just forces the above to get called.
FConsoleVariableSinkHandle MaxRepArraySizeHandle = CreateMaxArraySizeCVarAndRegisterSink();
FConsoleVariableSinkHandle MaxRepArrayMemorySink = CreateMaxArrayMemoryCVarAndRegisterSink();

#define ENABLE_PROPERTY_CHECKSUMS

//#define SANITY_CHECK_MERGES

#define USE_CUSTOM_COMPARE

//#define ENABLE_SUPER_CHECKSUMS

#ifdef USE_CUSTOM_COMPARE
static FORCEINLINE bool CompareBool( const FRepLayoutCmd& Cmd, const void* A, const void* B )
{
	return Cmd.Property->Identical( A, B );
}

static FORCEINLINE bool CompareObject( const FRepLayoutCmd& Cmd, const void* A, const void* B )
{
#if 1
	// Until UObjectPropertyBase::Identical is made safe for GC'd objects, we need to do it manually
	// This saves us from having to add referenced objects during GC
	UObjectPropertyBase * ObjProperty = CastChecked< UObjectPropertyBase>( Cmd.Property );

	UObject* ObjectA = ObjProperty->GetObjectPropertyValue( A );
	UObject* ObjectB = ObjProperty->GetObjectPropertyValue( B );

	return ObjectA == ObjectB;
#else
	return Cmd.Property->Identical( A, B );
#endif
}

template< typename T >
bool CompareValue( const T * A, const T * B )
{
	return *A == *B;
}

template< typename T >
bool CompareValue( const void* A, const void* B )
{
	return CompareValue( (T*)A, (T*)B);
}

static FORCEINLINE bool PropertiesAreIdenticalNative( const FRepLayoutCmd& Cmd, const void* A, const void* B )
{
	switch ( Cmd.Type )
	{
		case REPCMD_PropertyBool:			return CompareBool( Cmd, A, B );
		case REPCMD_PropertyByte:			return CompareValue<uint8>( A, B );
		case REPCMD_PropertyFloat:			return CompareValue<float>( A, B );
		case REPCMD_PropertyInt:			return CompareValue<int32>( A, B );
		case REPCMD_PropertyName:			return CompareValue<FName>( A, B );
		case REPCMD_PropertyObject:			return CompareObject( Cmd, A, B );
		case REPCMD_PropertyUInt32:			return CompareValue<uint32>( A, B );
		case REPCMD_PropertyUInt64:			return CompareValue<uint64>( A, B );
		case REPCMD_PropertyVector:			return CompareValue<FVector>( A, B );
		case REPCMD_PropertyVector100:		return CompareValue<FVector_NetQuantize100>( A, B );
		case REPCMD_PropertyVectorQ:		return CompareValue<FVector_NetQuantize>( A, B );
		case REPCMD_PropertyVectorNormal:	return CompareValue<FVector_NetQuantizeNormal>( A, B );
		case REPCMD_PropertyVector10:		return CompareValue<FVector_NetQuantize10>( A, B );
		case REPCMD_PropertyPlane:			return CompareValue<FPlane>( A, B );
		case REPCMD_PropertyRotator:		return CompareValue<FRotator>( A, B );
		case REPCMD_PropertyNetId:			return CompareValue<FUniqueNetIdRepl>( A, B );
		case REPCMD_RepMovement:			return CompareValue<FRepMovement>( A, B );
		case REPCMD_PropertyString:			return CompareValue<FString>( A, B );
		case REPCMD_Property:				return Cmd.Property->Identical( A, B );
		default: 
			UE_LOG( LogRep, Fatal, TEXT( "PropertiesAreIdentical: Unsupported type! %i (%s)" ), Cmd.Type, *Cmd.Property->GetName() );
	}

	return false;
}

static FORCEINLINE bool PropertiesAreIdentical( const FRepLayoutCmd& Cmd, const void* A, const void* B )
{
	const bool bIsIdentical = PropertiesAreIdenticalNative( Cmd, A, B );
#if 0
	// Sanity check result
	if ( bIsIdentical != Cmd.Property->Identical( A, B ) )
	{
		UE_LOG( LogRep, Fatal, TEXT( "PropertiesAreIdentical: Result mismatch! (%s)" ), *Cmd.Property->GetFullName() );
	}
#endif
	return bIsIdentical;
}
#else
static FORCEINLINE bool PropertiesAreIdentical( const FRepLayoutCmd& Cmd, const void* A, const void* B )
{
	return Cmd.Property->Identical( A, B );
}
#endif

static FORCEINLINE void StoreProperty( const FRepLayoutCmd& Cmd, void* A, const void* B )
{
	Cmd.Property->CopySingleValue( A, B );
}

static FORCEINLINE void SerializeGenericChecksum( FArchive & Ar )
{
	uint32 Checksum = 0xABADF00D;
	Ar << Checksum;
	check( Checksum == 0xABADF00D );
}

static void SerializeReadWritePropertyChecksum( const FRepLayoutCmd& Cmd, const int32 CurCmdIndex, const uint8* Data, FArchive & Ar )
{
	// Serialize various attributes that will mostly ensure we are working on the same property
	const uint32 NameHash = GetTypeHash( Cmd.Property->GetName() );

	uint32 MarkerChecksum = 0;

	// Evolve the checksum over several values that will uniquely identity where we are and should be
	MarkerChecksum = FCrc::MemCrc_DEPRECATED( &NameHash,		sizeof( NameHash ),		MarkerChecksum );
	MarkerChecksum = FCrc::MemCrc_DEPRECATED( &Cmd.Offset,		sizeof( Cmd.Offset ),	MarkerChecksum );
	MarkerChecksum = FCrc::MemCrc_DEPRECATED( &CurCmdIndex,	sizeof( CurCmdIndex ),	MarkerChecksum );

	const uint32 OriginalMarkerChecksum = MarkerChecksum;

	Ar << MarkerChecksum;

	if ( MarkerChecksum != OriginalMarkerChecksum )
	{
		// This is fatal, as it means we are out of sync to the point we can't recover
		UE_LOG( LogRep, Fatal, TEXT( "SerializeReadWritePropertyChecksum: Property checksum marker failed! [%s]" ), *Cmd.Property->GetFullName() );
	}

	if ( Cmd.Property->IsA( UObjectPropertyBase::StaticClass() ) )
	{
		// Can't handle checksums for objects right now
		// Need to resolve how to handle unmapped objects
		return;
	}

	// Now generate a checksum that guarantee that this property is in the exact state as the server
	// This will require NetSerializeItem to be deterministic, in and out
	// i.e, not only does NetSerializeItem need to write the same blob on the same input data, but
	//	it also needs to write the same blob it just read as well.
	FBitWriter Writer( 0, true );

	Cmd.Property->NetSerializeItem( Writer, NULL, const_cast< uint8* >( Data ) );

	if ( Ar.IsSaving() )
	{
		// If this is the server, do a read, and then another write so that we do exactly what the client will do, which will better ensure determinism 

		// We do this to force InitializeValue, DestroyValue etc to work on a single item
		const int32 OriginalDim = Cmd.Property->ArrayDim;
		Cmd.Property->ArrayDim = 1;

		TArray< uint8 > TempPropMemory;
		TempPropMemory.AddZeroed( Cmd.Property->ElementSize + 4 );
		uint32* Guard = (uint32*)&TempPropMemory[TempPropMemory.Num() - 4];
		const uint32 TAG_VALUE = 0xABADF00D;
		*Guard = TAG_VALUE;
		Cmd.Property->InitializeValue( TempPropMemory.GetData() );
		check( *Guard == TAG_VALUE );

		// Read it back in and then write it out to produce what the client will produce
		FBitReader Reader( Writer.GetData(), Writer.GetNumBits() );
		Cmd.Property->NetSerializeItem( Reader, NULL, TempPropMemory.GetData() );
		check( Reader.AtEnd() && !Reader.IsError() );
		check( *Guard == TAG_VALUE );

		// Write it back out for a final time
		Writer.Reset();

		Cmd.Property->NetSerializeItem( Writer, NULL, TempPropMemory.GetData() );
		check( *Guard == TAG_VALUE );

		// Destroy temp memory
		Cmd.Property->DestroyValue( TempPropMemory.GetData() );

		// Restore the static array size
		Cmd.Property->ArrayDim = OriginalDim;

		check( *Guard == TAG_VALUE );
	}

	uint32 PropertyChecksum = FCrc::MemCrc_DEPRECATED( Writer.GetData(), Writer.GetNumBytes() );

	const uint32 OriginalPropertyChecksum = PropertyChecksum;

	Ar << PropertyChecksum;

	if ( PropertyChecksum != OriginalPropertyChecksum )
	{
		// This is a warning, because for some reason, float rounding issues in the quantization functions cause this to return false positives
		UE_LOG( LogRep, Warning, TEXT( "Property checksum failed! [%s]" ), *Cmd.Property->GetFullName() );
	}
}

#define INIT_STACK( TStack )							\
	void InitStack( TStack& StackState )				\

#define SHOULD_PROCESS_NEXT_CMD()						\
	bool ShouldProcessNextCmd()							\

#define PROCESS_ARRAY_CMD( TStack )						\
	void ProcessArrayCmd_r(								\
	TStack&							PrevStackState,		\
	TStack&							StackState,			\
	const FRepLayoutCmd&			Cmd,				\
	const int32						CmdIndex,			\
	uint8* RESTRICT					ShadowData,			\
	uint8* RESTRICT					Data )				\


#define PROCESS_CMD( TStack )							\
	void ProcessCmd(									\
	TStack&							StackState,			\
	const FRepLayoutCmd&			Cmd,				\
	const int32						CmdIndex,			\
	uint8* RESTRICT					ShadowData,			\
	uint8* RESTRICT					Data )				\

class FCmdIteratorBaseStackState
{
public:
	FCmdIteratorBaseStackState( const int32 InCmdStart, const int32 InCmdEnd, FScriptArray*	InShadowArray, FScriptArray* InDataArray, uint8* RESTRICT InShadowBaseData, uint8* RESTRICT	InBaseData ) : 
		CmdStart( InCmdStart ),
		CmdEnd( InCmdEnd ),
		ShadowArray( InShadowArray ),
		DataArray( InDataArray ),
		ShadowBaseData( InShadowBaseData ),
		BaseData( InBaseData )
	{
	}

	const int32			CmdStart; 
	const int32			CmdEnd;

	FScriptArray*		ShadowArray;
	FScriptArray*		DataArray;

	uint8* RESTRICT		ShadowBaseData;
	uint8* RESTRICT		BaseData;
};

// This uses the "Curiously recurring template pattern" (CRTP) ideas
template< typename TImpl, typename TStackState >
class FRepLayoutCmdIterator
{
public:
	FRepLayoutCmdIterator( const TArray< FRepParentCmd >& InParents, const TArray< FRepLayoutCmd >& InCmds ) : Parents( InParents ), Cmds( InCmds ) {}

	void ProcessDataArrayElements_r( TStackState& StackState, const FRepLayoutCmd& Cmd )
	{
		const int32 NumDataArrayElements	= StackState.DataArray		? StackState.DataArray->Num()	: 0;
		const int32 NumShadowArrayElements	= StackState.ShadowArray	? StackState.ShadowArray->Num() : 0;

		// Loop using the number of elements in data array
		for ( int32 i = 0; i < NumDataArrayElements; i++ )
		{
			const int32 ElementOffset = i * Cmd.ElementSize;

			uint8* Data			= StackState.BaseData + ElementOffset;
			uint8* ShadowData	= i < NumShadowArrayElements ? ( StackState.ShadowBaseData + ElementOffset ) : NULL;	// ShadowArray might be smaller than DataArray

			ProcessCmds_r( StackState, ShadowData, Data );
		}
	}

	void ProcessShadowArrayElements_r( TStackState& StackState, const FRepLayoutCmd& Cmd )
	{
		const int32 NumDataArrayElements	= StackState.DataArray		? StackState.DataArray->Num()	: 0;
		const int32 NumShadowArrayElements	= StackState.ShadowArray	? StackState.ShadowArray->Num() : 0;

		// Loop using the number of elements in shadow array
		for ( int32 i = 0; i < NumShadowArrayElements; i++ )
		{
			const int32 ElementOffset = i * Cmd.ElementSize;

			uint8* Data			= i < NumDataArrayElements ? ( StackState.BaseData + ElementOffset ) : NULL;	// DataArray might be smaller than ShadowArray
			uint8* ShadowData	= StackState.ShadowBaseData + ElementOffset;

			ProcessCmds_r( StackState, ShadowData, Data );
		}
	}

	void ProcessArrayCmd_r( TStackState & PrevStackState, const FRepLayoutCmd& Cmd, const int32 CmdIndex, uint8* RESTRICT ShadowData, uint8* RESTRICT Data )
	{
		check( ShadowData != NULL || Data != NULL );

		FScriptArray* ShadowArray	= (FScriptArray*)ShadowData;
		FScriptArray* DataArray		= (FScriptArray*)Data;

		TStackState StackState( CmdIndex + 1, Cmd.EndCmd - 1, ShadowArray, DataArray, ShadowArray ? (uint8*)ShadowArray->GetData() : NULL, DataArray ? (uint8*)DataArray->GetData() : NULL );

		static_cast< TImpl* >( this )->ProcessArrayCmd_r( PrevStackState, StackState, Cmd, CmdIndex, ShadowData, Data );
	}

	void ProcessCmds_r( TStackState& StackState, uint8* RESTRICT ShadowData, uint8* RESTRICT Data )
	{
		check( ShadowData != NULL || Data != NULL );

		for ( int32 CmdIndex = StackState.CmdStart; CmdIndex < StackState.CmdEnd; CmdIndex++ )
		{
			const FRepLayoutCmd& Cmd = Cmds[ CmdIndex ];

			check( Cmd.Type != REPCMD_Return );

			if ( Cmd.Type == REPCMD_DynamicArray )
			{
				if ( static_cast< TImpl* >( this )->ShouldProcessNextCmd() )
				{
					ProcessArrayCmd_r( StackState, Cmd, CmdIndex, ShadowData ? ( ShadowData + Cmd.Offset ) : NULL, Data ? ( Data + Cmd.Offset ) : NULL );
				}
				CmdIndex = Cmd.EndCmd - 1;	// Jump past children of this array (-1 for ++ in for loop)
			}
			else
			{
				if ( static_cast< TImpl* >( this )->ShouldProcessNextCmd() )
				{
					static_cast< TImpl* >( this )->ProcessCmd( StackState, Cmd, CmdIndex, ShadowData, Data );
				}
			}
		}
	}

	void ProcessCmds( uint8* RESTRICT Data, uint8* RESTRICT ShadowData )
	{
		TStackState StackState( 0, Cmds.Num() - 1, NULL, NULL, ShadowData, Data );

		static_cast< TImpl* >( this )->InitStack( StackState );

		ProcessCmds_r( StackState, ShadowData, Data );
	}

	const TArray< FRepParentCmd >&	Parents;
	const TArray< FRepLayoutCmd >&	Cmds;
};

uint16 FRepLayout::CompareProperties_r(
	const int32				CmdStart,
	const int32				CmdEnd,
	const uint8* RESTRICT	CompareData,
	const uint8* RESTRICT	Data,
	TArray< uint16 > &		Changed,
	uint16					Handle,
	const bool				bIsInitial,
	const bool				bForceFail ) const
{
	check( CompareData );

	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];
		const FRepParentCmd& ParentCmd = Parents[Cmd.ParentIndex];

		check( Cmd.Type != REPCMD_Return );

		Handle++;

		const bool bIsLifetime = ( ParentCmd.Flags & PARENT_IsLifetime ) ? true : false;
		const bool bShouldSkip = !bIsLifetime || ( ParentCmd.Condition == COND_InitialOnly && !bIsInitial );

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			if ( bShouldSkip )
			{
				CmdIndex = Cmd.EndCmd - 1;		// The -1 to handle the ++ in the for loop
				continue;
			}

			// Once we hit an array, start using a stack based approach
			CompareProperties_Array_r( CompareData + Cmd.Offset, ( const uint8* )Data + Cmd.Offset, Changed, CmdIndex, Handle, bIsInitial, bForceFail );
			CmdIndex = Cmd.EndCmd - 1;		// The -1 to handle the ++ in the for loop
			continue;
		}

		if ( bShouldSkip )
		{
			continue;
		}

		if ( bForceFail || !PropertiesAreIdentical( Cmd, ( const void* )( CompareData + Cmd.Offset ), ( const void* )( Data + Cmd.Offset ) ) )
		{
			StoreProperty( Cmd, ( void* )( CompareData + Cmd.Offset ), ( const void* )( Data + Cmd.Offset ) );
			Changed.Add( Handle );
		}
	}

	return Handle;
}

void FRepLayout::CompareProperties_Array_r(
	const uint8* RESTRICT	CompareData,
	const uint8* RESTRICT	Data,
	TArray< uint16 > &		Changed,
	const uint16			CmdIndex,
	const uint16			Handle,
	const bool				bIsInitial,
	const bool				bForceFail
	) const
{
	const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

	FScriptArray * CompareArray = ( FScriptArray * )CompareData;
	FScriptArray * Array = ( FScriptArray * )Data;

	const uint16 ArrayNum			= Array->Num();
	const uint16 CompareArrayNum	= CompareArray->Num();

	// Make the shadow state match the actual state at the time of compare
	FScriptArrayHelper StoredArrayHelper( ( UArrayProperty * )Cmd.Property, CompareData );
	StoredArrayHelper.Resize( ArrayNum );

	TArray< uint16 > ChangedLocal;

	uint16 LocalHandle = 0;

	Data = ( uint8* )Array->GetData();
	CompareData = ( uint8* )CompareArray->GetData();

	for ( int32 i = 0; i < ArrayNum; i++ )
	{
		const int32 ElementOffset = i * Cmd.ElementSize;

		const bool bNewForceFail = bForceFail || i >= CompareArrayNum;

		LocalHandle = CompareProperties_r( CmdIndex + 1, Cmd.EndCmd - 1, CompareData + ElementOffset, Data + ElementOffset, ChangedLocal, LocalHandle, bIsInitial, bNewForceFail );
	}

	if ( ChangedLocal.Num() )
	{
		Changed.Add( Handle );
		Changed.Add( ChangedLocal.Num() );		// This is so we can jump over the array if we need to
		Changed.Append( ChangedLocal );
		Changed.Add( 0 );
	}
	else if ( ArrayNum != CompareArrayNum )
	{
		// If nothing below us changed, we either shrunk, or we grew and our inner was an array that didn't have any elements
		check( ArrayNum < CompareArrayNum || Cmds[CmdIndex + 1].Type == REPCMD_DynamicArray );

		// Array got smaller, send the array handle to force array size change
		Changed.Add( Handle );
		Changed.Add( 0 );
		Changed.Add( 0 );
	}
}

bool FRepLayout::CompareProperties(
	FRepChangelistState* RESTRICT	RepChangelistState,
	const uint8* RESTRICT			Data,
	const FReplicationFlags&		RepFlags ) const
{
	SCOPE_CYCLE_COUNTER( STAT_NetReplicateDynamicPropTime );

	RepChangelistState->CompareIndex++;

	check( RepChangelistState->HistoryEnd - RepChangelistState->HistoryStart < FRepChangelistState::MAX_CHANGE_HISTORY );
	const int32 HistoryIndex = RepChangelistState->HistoryEnd % FRepChangelistState::MAX_CHANGE_HISTORY;

	FRepChangedHistory& NewHistoryItem = RepChangelistState->ChangeHistory[HistoryIndex];

	TArray<uint16>& Changed = NewHistoryItem.Changed;
	Changed.Empty();

	CompareProperties_r( 0, Cmds.Num() - 1, RepChangelistState->StaticBuffer.GetData(), Data, Changed, 0, RepFlags.bNetInitial, false );

	if ( Changed.Num() == 0 )
	{
		return false;
	}

	//
	// We produced a new change list, copy it to the history
	//

	// Null terminator
	Changed.Add( 0 );

	// Move end pointer
	RepChangelistState->HistoryEnd++;

	// If we're full, merge the oldest up, so we always have room for a new entry
	if ( RepChangelistState->HistoryEnd - RepChangelistState->HistoryStart == FRepChangelistState::MAX_CHANGE_HISTORY )
	{
		const int32 FirstHistoryIndex = RepChangelistState->HistoryStart % FRepChangelistState::MAX_CHANGE_HISTORY;

		RepChangelistState->HistoryStart++;

		const int32 SecondHistoryIndex = RepChangelistState->HistoryStart % FRepChangelistState::MAX_CHANGE_HISTORY;

		TArray< uint16 >& FirstChangelistRef = RepChangelistState->ChangeHistory[FirstHistoryIndex].Changed;
		TArray< uint16 > SecondChangelistCopy = RepChangelistState->ChangeHistory[SecondHistoryIndex].Changed;

		MergeChangeList( Data, FirstChangelistRef, SecondChangelistCopy, RepChangelistState->ChangeHistory[SecondHistoryIndex].Changed );
	}

	return true;
}

static FORCEINLINE void WritePropertyHandle( FNetBitWriter & Writer, uint16 Handle, bool bDoChecksum )
{
	const int NumStartingBits = Writer.GetNumBits();

	uint32 LocalHandle = Handle;
	Writer.SerializeIntPacked( LocalHandle );

#ifdef ENABLE_PROPERTY_CHECKSUMS
	if ( bDoChecksum )
	{
		SerializeGenericChecksum( Writer );
	}
#endif

	NETWORK_PROFILER( GNetworkProfiler.TrackWritePropertyHandle( Writer.GetNumBits() - NumStartingBits, nullptr ) );
}

bool FRepLayout::ReplicateProperties(
	FRepState* RESTRICT				RepState,
	FRepChangelistState* RESTRICT	RepChangelistState,
	const uint8* RESTRICT			Data,
	UClass*							ObjectClass,
	UActorChannel*					OwningChannel,
	FNetBitWriter&					Writer,
	const FReplicationFlags &		RepFlags ) const
{
	SCOPE_CYCLE_COUNTER( STAT_NetReplicateDynamicPropTime );

	check( ObjectClass == Owner );

	FRepChangedPropertyTracker*	ChangeTracker = RepState->RepChangedPropertyTracker.Get();

	// Rebuild conditional state if needed
	if ( RepState->RepFlags.Value != RepFlags.Value )
	{
		RebuildConditionalProperties( RepState, *ChangeTracker, RepFlags );

		RepState->RepFlags.Value = RepFlags.Value;
	}

	if ( OwningChannel->Connection->bResendAllDataSinceOpen )
	{
		check( OwningChannel->Connection->InternalAck );
		// If we are resending data since open, we don't want to affect the current state of channel/replication, so just do the minimum and send the data, and return
		if ( RepState->LifetimeChangelist.Num() > 0 )
		{
			// Use a pruned version of the list, in case arrays changed size since the last time we replicated
			TArray< uint16 > Pruned;
			PruneChangeList( RepState, Data, RepState->LifetimeChangelist, Pruned );
			RepState->LifetimeChangelist = MoveTemp( Pruned );
			SendProperties_BackwardsCompatible( RepState, ChangeTracker, Data, OwningChannel->Connection, Writer, RepState->LifetimeChangelist );
			return true;
		}

		return false;
	}

	check( RepState->HistoryEnd >= RepState->HistoryStart );
	check( RepState->HistoryEnd - RepState->HistoryStart < FRepState::MAX_CHANGE_HISTORY );

	const bool bFlushPreOpenAckHistory = RepState->OpenAckedCalled && RepState->PreOpenAckHistory.Num() > 0;

	const bool bCompareIndexSame = RepState->LastCompareIndex == RepChangelistState->CompareIndex;

	RepState->LastCompareIndex = RepChangelistState->CompareIndex;

	// We can early out if we know for sure there are no new changelists to send
	if ( bCompareIndexSame || RepState->LastChangelistIndex == RepChangelistState->HistoryEnd )
	{
		if ( RepState->NumNaks == 0 && !bFlushPreOpenAckHistory )
		{
			// Nothing changed and there are no nak's, so just do normal housekeeping and remove acked history items
			UpdateChangelistHistory( RepState, ObjectClass, Data, OwningChannel->Connection, NULL );
			return false;
		}
	}

	// Clamp to the valid history range (and warn if we end up sending entire history, this should only happen if we get really far behind)
	//	NOTE - The RepState->LastChangelistIndex != 0 should handle/ignore the JIP case
	if ( RepState->LastChangelistIndex <= RepChangelistState->HistoryStart )
	{
		if ( RepState->LastChangelistIndex != 0 )
		{
			UE_LOG( LogRep, Warning, TEXT( "FRepLayout::ReplicatePropertiesUsingChangelistState: Entire history sent for: %s" ), *GetNameSafe( ObjectClass ) );
		}

		RepState->LastChangelistIndex = RepChangelistState->HistoryStart;
	}

	const int32 PossibleNewHistoryIndex = RepState->HistoryEnd % FRepState::MAX_CHANGE_HISTORY;

	FRepChangedHistory& PossibleNewHistoryItem = RepState->ChangeHistory[PossibleNewHistoryIndex];

	TArray< uint16 >& Changed = PossibleNewHistoryItem.Changed;

	check( Changed.Num() == 0 );		// Make sure this history item is actually inactive

	// Gather all change lists that are new since we last looked, and merge them all together into a single CL
	for ( int32 i = RepState->LastChangelistIndex; i < RepChangelistState->HistoryEnd; i++ )
	{
		const int32 HistoryIndex = i % FRepChangelistState::MAX_CHANGE_HISTORY;

		FRepChangedHistory& HistoryItem = RepChangelistState->ChangeHistory[HistoryIndex];

		TArray< uint16 > Temp = Changed;
		MergeChangeList( Data, HistoryItem.Changed, Temp, Changed );
	}

	// We're all caught up now
	RepState->LastChangelistIndex = RepChangelistState->HistoryEnd;

	if ( Changed.Num() > 0 || RepState->NumNaks > 0 || bFlushPreOpenAckHistory )
	{
		RepState->HistoryEnd++;

		UpdateChangelistHistory( RepState, ObjectClass, Data, OwningChannel->Connection, &Changed );

		// Merge in the PreOpenAckHistory (unreliable properties sent before the bunch was initially acked)
		if ( bFlushPreOpenAckHistory )
		{
			for ( int32 i = 0; i < RepState->PreOpenAckHistory.Num(); i++ )
			{
				TArray< uint16 > Temp = Changed;
				Changed.Empty();
				MergeChangeList( Data, RepState->PreOpenAckHistory[i].Changed, Temp, Changed );
			}
			RepState->PreOpenAckHistory.Empty();
		}
	}
	else
	{
		// Nothing changed and there are no nak's, so just do normal housekeeping and remove acked history items
		UpdateChangelistHistory( RepState, ObjectClass, Data, OwningChannel->Connection, NULL );
		return false;		// Nothing to send
	}

	// At this point we should have a non empty change list
	check( Changed.Num() > 0 );

	const int32 NumBits = Writer.GetNumBits();

	// Send the final merged change list
	if ( OwningChannel->Connection->InternalAck )
	{
		// Remember all properties that have changed since this channel was first opened in case we need it (for bResendAllDataSinceOpen)
		TArray< uint16 > Temp = RepState->LifetimeChangelist;
		MergeChangeList( Data, Changed, Temp, RepState->LifetimeChangelist );

		SendProperties_BackwardsCompatible( RepState, ChangeTracker, Data, OwningChannel->Connection, Writer, Changed );
	}
	else
	{
		SendProperties( RepState, ChangeTracker, Data, ObjectClass, Writer, Changed );
	}

	// See if something actually sent (this may be false due to conditional checks inside the send properties function
	const bool bSomethingSent = NumBits != Writer.GetNumBits();

	if ( !bSomethingSent )
	{
		// We need to revert the change list in the history if nothing really sent (can happen due to condition checks)
		Changed.Empty();
		RepState->HistoryEnd--;
	}

	return bSomethingSent;
}

void FRepLayout::UpdateChangelistHistory( FRepState * RepState, UClass * ObjectClass, const uint8* RESTRICT Data, UNetConnection* Connection, TArray< uint16 > * OutMerged ) const
{
	check( RepState->HistoryEnd >= RepState->HistoryStart );

	const int32 HistoryCount	= RepState->HistoryEnd - RepState->HistoryStart;
	const bool DumpHistory		= HistoryCount == FRepState::MAX_CHANGE_HISTORY;
	const int32 AckPacketId		= Connection->OutAckPacketId;

	// If our buffer is currently full, forcibly send the entire history
	if ( DumpHistory )
	{
		UE_LOG( LogRep, Log, TEXT( "FRepLayout::UpdateChangelistHistory: History overflow, forcing history dump %s, %s" ), *ObjectClass->GetName(), *Connection->Describe());
	}

	for ( int32 i = RepState->HistoryStart; i < RepState->HistoryEnd; i++ )
	{
		const int32 HistoryIndex = i % FRepState::MAX_CHANGE_HISTORY;

		FRepChangedHistory & HistoryItem = RepState->ChangeHistory[ HistoryIndex ];

		if ( HistoryItem.OutPacketIdRange.First == INDEX_NONE )
		{
			continue;		//  Hasn't been initialized in PostReplicate yet
		}

		check( HistoryItem.Changed.Num() > 0 );		// All active history items should contain a change list

		if ( AckPacketId >= HistoryItem.OutPacketIdRange.Last || HistoryItem.Resend || DumpHistory )
		{
			if ( HistoryItem.Resend || DumpHistory )
			{
				// Merge in nak'd change lists
				check( OutMerged != NULL );
				TArray< uint16 > Temp = *OutMerged;
				OutMerged->Empty();
				MergeChangeList( Data, HistoryItem.Changed, Temp, *OutMerged );
				HistoryItem.Changed.Empty();

#ifdef SANITY_CHECK_MERGES
				SanityCheckChangeList( Data, *OutMerged );
#endif

				if ( HistoryItem.Resend )
				{
					HistoryItem.Resend = false;
					RepState->NumNaks--;
				}
			}

			HistoryItem.Changed.Empty();
			HistoryItem.OutPacketIdRange = FPacketIdRange();
			RepState->HistoryStart++;
		}
	}

	// Remove any tiling in the history markers to keep them from wrapping over time
	const int32 NewHistoryCount	= RepState->HistoryEnd - RepState->HistoryStart;

	check( NewHistoryCount <= FRepState::MAX_CHANGE_HISTORY );

	RepState->HistoryStart	= RepState->HistoryStart % FRepState::MAX_CHANGE_HISTORY;
	RepState->HistoryEnd	= RepState->HistoryStart + NewHistoryCount;

	check( RepState->NumNaks == 0 );	// Make sure we processed all the naks properly
}

void FRepLayout::OpenAcked( FRepState * RepState ) const
{
	check( RepState != NULL );
	RepState->OpenAckedCalled = true;
}

void FRepLayout::PostReplicate( FRepState * RepState, FPacketIdRange & PacketRange, bool bReliable ) const
{
	for ( int32 i = RepState->HistoryStart; i < RepState->HistoryEnd; i++ )
	{
		const int32 HistoryIndex = i % FRepState::MAX_CHANGE_HISTORY;

		FRepChangedHistory & HistoryItem = RepState->ChangeHistory[ HistoryIndex ];

		if ( HistoryItem.OutPacketIdRange.First == INDEX_NONE )
		{
			check( HistoryItem.Changed.Num() > 0 );
			check( !HistoryItem.Resend );

			HistoryItem.OutPacketIdRange = PacketRange;

			if ( !bReliable && !RepState->OpenAckedCalled )
			{
				RepState->PreOpenAckHistory.Add( HistoryItem );
			}
		}
	}
}

void FRepLayout::ReceivedNak( FRepState * RepState, int32 NakPacketId ) const
{
	if ( RepState == NULL )
	{
		return;		// I'm not 100% certain why this happens, the only think I can think of is this is a bNetTemporary?
	}

	for ( int32 i = RepState->HistoryStart; i < RepState->HistoryEnd; i++ )
	{
		const int32 HistoryIndex = i % FRepState::MAX_CHANGE_HISTORY;

		FRepChangedHistory & HistoryItem = RepState->ChangeHistory[ HistoryIndex ];

		if ( !HistoryItem.Resend && HistoryItem.OutPacketIdRange.InRange( NakPacketId ) )
		{
			check( HistoryItem.Changed.Num() > 0 );
			HistoryItem.Resend = true;
			RepState->NumNaks++;
		}
	}
}

bool FRepLayout::AllAcked( FRepState * RepState ) const
{	
	if ( RepState->HistoryStart != RepState->HistoryEnd )
	{
		// We have change lists that haven't been acked
		return false;
	}

	if ( RepState->NumNaks > 0 )
	{
		return false;
	}

	if ( !RepState->OpenAckedCalled )
	{
		return false;
	}

	if ( RepState->PreOpenAckHistory.Num() > 0 )
	{
		return false;
	}

	return true;
}

bool FRepLayout::ReadyForDormancy( FRepState * RepState ) const
{
	if ( RepState == NULL )
	{
		return false;
	}

	return AllAcked( RepState );
}

void FRepLayout::SerializeObjectReplicatedProperties(UObject* Object, FArchive & Ar) const
{
	for (int32 i = 0; i < Parents.Num(); i++)
	{
        UStructProperty* StructProperty = Cast<UStructProperty>(Parents[i].Property);
        UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Parents[i].Property);

		// We're only able to easily serialize non-object/struct properties, so just do those.
		if (ObjectProperty == nullptr && StructProperty == nullptr)
		{
			bool bHasUnmapped = false;
			SerializeProperties_r(Ar, NULL, Parents[i].CmdStart, Parents[i].CmdEnd, (uint8*)Object, bHasUnmapped);
		}
	}
}

bool FRepHandleIterator::NextHandle()
{
	CmdIndex = INDEX_NONE;

	Handle = ChangelistIterator.Changed[ChangelistIterator.ChangedIndex];

	if ( Handle == 0 )
	{
		return false;		// Done
	}

	ChangelistIterator.ChangedIndex++;

	if ( !ensure( ChangelistIterator.ChangedIndex < ChangelistIterator.Changed.Num() ) )
	{
		return false;
	}

	const int32 HandleMinusOne = Handle - 1;

	ArrayIndex = ( ArrayElementSize > 0 && NumHandlesPerElement > 0 ) ? HandleMinusOne / NumHandlesPerElement : 0;

	if ( ArrayIndex >= MaxArrayIndex )
	{
		return false;
	}

	ArrayOffset	= ArrayIndex * ArrayElementSize;

	const int32 RelativeHandle = HandleMinusOne - ArrayIndex * NumHandlesPerElement;

	CmdIndex = HandleToCmdIndex[RelativeHandle].CmdIndex;

	if ( !ensure( CmdIndex >= MinCmdIndex && CmdIndex < MaxCmdIndex ) )
	{
		return false;
	}

	const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

	if ( !ensure( Cmd.RelativeHandle - 1 == RelativeHandle ) )
	{
		return false;
	}

	if ( !ensure( Cmd.Type != REPCMD_Return ) )
	{
		return false;
	}

	return true;
}

bool FRepHandleIterator::JumpOverArray()
{
	const int32 ArrayChangedCount = ChangelistIterator.Changed[ChangelistIterator.ChangedIndex++];
	ChangelistIterator.ChangedIndex += ArrayChangedCount;

	if ( !ensure( ChangelistIterator.Changed[ChangelistIterator.ChangedIndex] == 0 ) )
	{
		return false;
	}

	ChangelistIterator.ChangedIndex++;

	return true;
}

int32 FRepHandleIterator::PeekNextHandle() const
{
	return ChangelistIterator.Changed[ChangelistIterator.ChangedIndex];
}

class FScopedIteratorArrayTracker
{
public:
	FScopedIteratorArrayTracker( FRepHandleIterator* InCmdIndexIterator )
	{
		CmdIndexIterator = InCmdIndexIterator;

		if ( CmdIndexIterator )
		{
			ArrayChangedCount	= CmdIndexIterator->ChangelistIterator.Changed[CmdIndexIterator->ChangelistIterator.ChangedIndex++];
			OldChangedIndex		= CmdIndexIterator->ChangelistIterator.ChangedIndex;
		}
	}

	~FScopedIteratorArrayTracker()
	{
		if ( CmdIndexIterator )
		{
			check( CmdIndexIterator->ChangelistIterator.ChangedIndex - OldChangedIndex <= ArrayChangedCount );
			CmdIndexIterator->ChangelistIterator.ChangedIndex = OldChangedIndex + ArrayChangedCount;
			check( CmdIndexIterator->PeekNextHandle() == 0 );
			CmdIndexIterator->ChangelistIterator.ChangedIndex++;
		}
	}

	FRepHandleIterator*	CmdIndexIterator;
	int32				ArrayChangedCount;
	int32				OldChangedIndex;
};

void FRepLayout::MergeChangeList_r(
	FRepHandleIterator&		RepHandleIterator1,
	FRepHandleIterator&		RepHandleIterator2,
	const uint8* RESTRICT	SourceData,
	TArray< uint16 >&		OutChanged ) const
{
	while ( true )
	{
		const int32 NextHandle1 = RepHandleIterator1.PeekNextHandle();
		const int32 NextHandle2 = RepHandleIterator2.PeekNextHandle();

		if ( NextHandle1 == 0 && NextHandle2 == 0 )
		{
			break;		// Done
		}

		if ( NextHandle2 == 0 )
		{
			PruneChangeList_r( RepHandleIterator1, SourceData, OutChanged );
			return;
		}
		else if ( NextHandle1 == 0 )
		{
			PruneChangeList_r( RepHandleIterator2, SourceData, OutChanged );
			return;
		}

		FRepHandleIterator* ActiveIterator1 = nullptr;
		FRepHandleIterator* ActiveIterator2 = nullptr;

		int32 CmdIndex		= INDEX_NONE;
		int32 ArrayOffset	= INDEX_NONE;

		if ( NextHandle1 < NextHandle2 )
		{
			if ( !RepHandleIterator1.NextHandle() )
			{
				break;		// Array overflow
			}

			OutChanged.Add( NextHandle1 );

			CmdIndex	= RepHandleIterator1.CmdIndex;
			ArrayOffset = RepHandleIterator1.ArrayOffset;

			ActiveIterator1 = &RepHandleIterator1;
		}
		else if ( NextHandle2 < NextHandle1 )
		{
			if ( !RepHandleIterator2.NextHandle() )
			{
				break;		// Array overflow
			}

			OutChanged.Add( NextHandle2 );

			CmdIndex	= RepHandleIterator2.CmdIndex;
			ArrayOffset = RepHandleIterator2.ArrayOffset;

			ActiveIterator2 = &RepHandleIterator2;
		}
		else
		{
			check( NextHandle1 == NextHandle2 );

			if ( !RepHandleIterator1.NextHandle() )
			{
				break;		// Array overflow
			}

			if ( !ensure( RepHandleIterator2.NextHandle() ) )
			{
				break;		// Array overflow
			}

			check( RepHandleIterator1.CmdIndex == RepHandleIterator2.CmdIndex );

			OutChanged.Add( NextHandle1 );

			CmdIndex	= RepHandleIterator1.CmdIndex;
			ArrayOffset = RepHandleIterator1.ArrayOffset;

			ActiveIterator1 = &RepHandleIterator1;
			ActiveIterator2 = &RepHandleIterator2;
		}

		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			const uint8* Data = SourceData + ArrayOffset + Cmd.Offset;

			const FScriptArray* Array = ( FScriptArray * )Data;

			FScopedIteratorArrayTracker ArrayTracker1( ActiveIterator1 );
			FScopedIteratorArrayTracker ArrayTracker2( ActiveIterator2 );

			const int32 OriginalChangedNum	= OutChanged.AddUninitialized();

			const uint8* NewData = ( uint8* )Array->GetData();

			TArray< FHandleToCmdIndex >& ArrayHandleToCmdIndex = ActiveIterator1 ? *ActiveIterator1->HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex : *ActiveIterator2->HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex; //-V595

			if ( !ActiveIterator1 )
			{
				FRepHandleIterator ArrayIterator2( ActiveIterator2->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1 );
				PruneChangeList_r( ArrayIterator2, NewData, OutChanged );
			}
			else if ( !ActiveIterator2 )
			{
				FRepHandleIterator ArrayIterator1( ActiveIterator1->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1 );
				PruneChangeList_r( ArrayIterator1, NewData, OutChanged );
			}
			else
			{
				FRepHandleIterator ArrayIterator1( ActiveIterator1->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1 );
				FRepHandleIterator ArrayIterator2( ActiveIterator2->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1 );

				MergeChangeList_r( ArrayIterator1, ArrayIterator2, NewData, OutChanged );
			}

			// Patch in the jump offset
			OutChanged[OriginalChangedNum] = OutChanged.Num() - ( OriginalChangedNum + 1 );

			// Add the array terminator
			OutChanged.Add( 0 );
		}
	}
}

void FRepLayout::PruneChangeList_r(
	FRepHandleIterator&		RepHandleIterator,
	const uint8* RESTRICT	SourceData,
	TArray< uint16 >&		OutChanged ) const
{
	while ( RepHandleIterator.NextHandle() )
	{
		OutChanged.Add( RepHandleIterator.Handle );

		const int32 CmdIndex	= RepHandleIterator.CmdIndex;
		const int32 ArrayOffset = RepHandleIterator.ArrayOffset;

		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			const uint8* Data = SourceData + ArrayOffset + Cmd.Offset;

			const FScriptArray* Array = ( FScriptArray * )Data;

			FScopedIteratorArrayTracker ArrayTracker( &RepHandleIterator );

			const int32 OriginalChangedNum = OutChanged.AddUninitialized();

			const uint8* NewData = ( uint8* )Array->GetData();

			TArray< FHandleToCmdIndex >& ArrayHandleToCmdIndex = *RepHandleIterator.HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex;

			FRepHandleIterator ArrayIterator( RepHandleIterator.ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1 );
			PruneChangeList_r( ArrayIterator, NewData, OutChanged );

			// Patch in the jump offset
			OutChanged[OriginalChangedNum] = OutChanged.Num() - ( OriginalChangedNum + 1 );

			// Add the array terminator
			OutChanged.Add( 0 );
		}
	}
}

void FRepLayout::SendProperties_r(
	FRepState*	RESTRICT				RepState,
	FRepChangedPropertyTracker*			ChangedTracker,
	FNetBitWriter&						Writer,
	const bool							bDoChecksum,
	FRepHandleIterator&					HandleIterator,
	const uint8* RESTRICT				SourceData ) const
{
	while ( HandleIterator.NextHandle() )
	{
		const FRepLayoutCmd& Cmd = Cmds[HandleIterator.CmdIndex];

		const FRepParentCmd& ParentCmd = Parents[Cmd.ParentIndex];

		if ( !RepState->ConditionMap[ParentCmd.Condition] || !ChangedTracker->Parents[Cmd.ParentIndex].Active )
		{
			if ( Cmd.Type == REPCMD_DynamicArray )
			{
				if ( !HandleIterator.JumpOverArray() )
				{
					break;
				}
			}

			continue;
		}

		const uint8* Data = SourceData + HandleIterator.ArrayOffset + Cmd.Offset;

		WritePropertyHandle( Writer, HandleIterator.Handle, bDoChecksum );

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			const FScriptArray* Array = ( FScriptArray * )Data;

			// Write array num
			uint16 ArrayNum = Array->Num();
			Writer << ArrayNum;

			// Read the jump offset
			// We won't need to actually jump over anything because we expect the change list to be pruned once we get here
			// But we can use it to verify we read the correct amount.
			const int32 ArrayChangedCount = HandleIterator.ChangelistIterator.Changed[HandleIterator.ChangelistIterator.ChangedIndex++];

			const int32 OldChangedIndex = HandleIterator.ChangelistIterator.ChangedIndex;

			const uint8* NewData = ( uint8* )Array->GetData();

			TArray< FHandleToCmdIndex >& ArrayHandleToCmdIndex = *HandleIterator.HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex;

			FRepHandleIterator ArrayHandleIterator( HandleIterator.ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, ArrayNum, HandleIterator.CmdIndex + 1, Cmd.EndCmd - 1 );

			check( ArrayHandleIterator.ArrayElementSize > 0 );
			check( ArrayHandleIterator.NumHandlesPerElement > 0 );

			SendProperties_r( RepState, ChangedTracker, Writer, bDoChecksum, ArrayHandleIterator, NewData );

			check( HandleIterator.ChangelistIterator.ChangedIndex - OldChangedIndex == ArrayChangedCount );				// Make sure we read correct amount
			check( HandleIterator.ChangelistIterator.Changed[HandleIterator.ChangelistIterator.ChangedIndex] == 0 );	// Make sure we are at the end

			HandleIterator.ChangelistIterator.ChangedIndex++;

			WritePropertyHandle( Writer, 0, bDoChecksum );		// Signify end of dynamic array
			continue;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( CVarDoReplicationContextString->GetInt() > 0 )
		{
			Writer.PackageMap->SetDebugContextString( FString::Printf( TEXT( "%s - %s" ), *Owner->GetPathName(), *Cmd.Property->GetPathName() ) );
		}
#endif

		const int32 NumStartBits = Writer.GetNumBits();

		// This property changed, so send it
		Cmd.Property->NetSerializeItem( Writer, Writer.PackageMap, ( void* )Data );

		const int32 NumEndBits = Writer.GetNumBits();

		NETWORK_PROFILER( GNetworkProfiler.TrackReplicateProperty( ParentCmd.Property, NumEndBits - NumStartBits, nullptr ) );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( CVarDoReplicationContextString->GetInt() > 0 )
		{
			Writer.PackageMap->ClearDebugContextString();
		}
#endif

#ifdef ENABLE_PROPERTY_CHECKSUMS
		if ( bDoChecksum )
		{
			SerializeReadWritePropertyChecksum( Cmd, HandleIterator.CmdIndex, Data, Writer );
		}
#endif
	}
}

void FRepLayout::SendProperties(
	FRepState *	RESTRICT		RepState,
	FRepChangedPropertyTracker* ChangedTracker,
	const uint8* RESTRICT		Data,
	UClass *					ObjectClass,
	FNetBitWriter&				Writer,
	TArray< uint16 > &			Changed ) const
{
#ifdef ENABLE_PROPERTY_CHECKSUMS
	const bool bDoChecksum = CVarDoPropertyChecksum.GetValueOnAnyThread() == 1;
#else
	const bool bDoChecksum = false;
#endif

	FBitWriterMark Mark( Writer );

#ifdef ENABLE_PROPERTY_CHECKSUMS
	Writer.WriteBit( bDoChecksum ? 1 : 0 );
#endif

	const int32 NumBits = Writer.GetNumBits();

	FChangelistIterator ChangelistIterator( Changed, 0 );
	FRepHandleIterator HandleIterator( ChangelistIterator, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1 );

	SendProperties_r( RepState, ChangedTracker, Writer, bDoChecksum, HandleIterator, Data );

	if ( NumBits != Writer.GetNumBits() )
	{
		// We actually wrote stuff
		WritePropertyHandle( Writer, 0, bDoChecksum );
	}
	else
	{
		Mark.Pop( Writer );
	}
}

static FORCEINLINE void WritePropertyHandle_BackwardsCompatible( FNetBitWriter & Writer, uint32 NetFieldExportHandle, bool bDoChecksum )
{
	const int NumStartingBits = Writer.GetNumBits();

	Writer.SerializeIntPacked( NetFieldExportHandle );

#ifdef ENABLE_PROPERTY_CHECKSUMS
	if ( bDoChecksum )
	{
		SerializeGenericChecksum( Writer );
	}
#endif

	NETWORK_PROFILER( GNetworkProfiler.TrackWritePropertyHandle( Writer.GetNumBits() - NumStartingBits, nullptr ) );
}

TSharedPtr< FNetFieldExportGroup > FRepLayout::CreateNetfieldExportGroup() const
{
	TSharedPtr< FNetFieldExportGroup > NetFieldExportGroup = TSharedPtr< FNetFieldExportGroup >( new FNetFieldExportGroup() );

	NetFieldExportGroup->PathName = Owner->GetPathName();
	NetFieldExportGroup->NetFieldExports.SetNum( Cmds.Num() );

	for ( int32 i = 0; i < Cmds.Num(); i++ )
	{
		FNetFieldExport NetFieldExport(
			i,
			Cmds[i].CompatibleChecksum,
			Cmds[i].Property ? Cmds[i].Property->GetName() : TEXT( "" ),
			Cmds[i].Property ? Cmds[i].Property->GetCPPType( nullptr, 0 ) : TEXT( "" ) );

		NetFieldExportGroup->NetFieldExports[i] = NetFieldExport;
	}

	return NetFieldExportGroup;
}

static FORCEINLINE void WriteProperty_BackwardsCompatible( FNetBitWriter& Writer, const FRepLayoutCmd& Cmd, const int32 CmdIndex, const UObject* Owner, const uint8* Data, const bool bDoChecksum )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( CVarDoReplicationContextString->GetInt() > 0 )
	{
		Writer.PackageMap->SetDebugContextString( FString::Printf( TEXT( "%s - %s" ), *Owner->GetPathName(), *Cmd.Property->GetPathName() ) );
	}
#endif

	const int32 NumStartBits = Writer.GetNumBits();

	FNetBitWriter TempWriter( Writer.PackageMap, 0 );

	// This property changed, so send it
	Cmd.Property->NetSerializeItem( TempWriter, TempWriter.PackageMap, ( void* )Data );

	uint32 NumBits = TempWriter.GetNumBits();
	Writer.SerializeIntPacked( NumBits );
	Writer.SerializeBits( TempWriter.GetData(), NumBits );

	const int32 NumEndBits = Writer.GetNumBits();

	NETWORK_PROFILER( GNetworkProfiler.TrackReplicateProperty( Cmd.Property, NumEndBits - NumStartBits, nullptr ) );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( CVarDoReplicationContextString->GetInt() > 0 )
	{
		Writer.PackageMap->ClearDebugContextString();
	}
#endif

#ifdef ENABLE_PROPERTY_CHECKSUMS
	if ( bDoChecksum )
	{
		SerializeReadWritePropertyChecksum( Cmd, CmdIndex, Data, Writer );
	}
#endif
}

void FRepLayout::SendProperties_BackwardsCompatible_r(
	FRepState* RESTRICT					RepState,
	UPackageMapClient*					PackageMapClient,
	FNetFieldExportGroup*				NetFieldExportGroup,
	FRepChangedPropertyTracker*			ChangedTracker,
	FNetBitWriter&						Writer,
	const bool							bDoChecksum,
	FRepHandleIterator&					HandleIterator,
	const uint8* RESTRICT				SourceData ) const
{
	int32 OldIndex = -1;

	while ( HandleIterator.NextHandle() )
	{
		const FRepLayoutCmd& Cmd = Cmds[HandleIterator.CmdIndex];

		const FRepParentCmd& ParentCmd = Parents[Cmd.ParentIndex];

		const bool bConditionMatches = ChangedTracker == nullptr || ( RepState->ConditionMap[ParentCmd.Condition] && ChangedTracker->Parents[Cmd.ParentIndex].Active );

		if ( !bConditionMatches )
		{
			if ( Cmd.Type == REPCMD_DynamicArray )
			{
				if ( !HandleIterator.JumpOverArray() )
				{
					break;
				}
			}

			continue;
		}

		const uint8* Data = SourceData + HandleIterator.ArrayOffset + Cmd.Offset;

		PackageMapClient->TrackNetFieldExport( NetFieldExportGroup, HandleIterator.CmdIndex );

		if ( HandleIterator.ArrayElementSize > 0 && HandleIterator.ArrayIndex != OldIndex )
		{
			if ( OldIndex != -1 )
			{
				WritePropertyHandle_BackwardsCompatible( Writer, 0, bDoChecksum );
			}

			uint32 Index = HandleIterator.ArrayIndex + 1;
			Writer.SerializeIntPacked( Index );
			OldIndex = HandleIterator.ArrayIndex;
		}

		WritePropertyHandle_BackwardsCompatible( Writer, HandleIterator.CmdIndex + 1, bDoChecksum );

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			const FScriptArray* Array = ( FScriptArray * )Data;

			uint32 ArrayNum = Array->Num();

			// Read the jump offset
			// We won't need to actually jump over anything because we expect the change list to be pruned once we get here
			// But we can use it to verify we read the correct amount.
			const int32 ArrayChangedCount = HandleIterator.ChangelistIterator.Changed[HandleIterator.ChangelistIterator.ChangedIndex++];

			const int32 OldChangedIndex = HandleIterator.ChangelistIterator.ChangedIndex;

			const uint8* NewData = ( uint8* )Array->GetData();

			TArray< FHandleToCmdIndex >& ArrayHandleToCmdIndex = *HandleIterator.HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex;

			FRepHandleIterator ArrayHandleIterator( HandleIterator.ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, ArrayNum, HandleIterator.CmdIndex + 1, Cmd.EndCmd - 1 );

			check( ArrayHandleIterator.ArrayElementSize > 0 );
			check( ArrayHandleIterator.NumHandlesPerElement > 0 );

			FNetBitWriter TempWriter( Writer.PackageMap, 0 );

			// Write array num
			TempWriter.SerializeIntPacked( ArrayNum );

			if ( ArrayNum > 0 )
			{
				SendProperties_BackwardsCompatible_r( RepState, PackageMapClient, NetFieldExportGroup, ChangedTracker, TempWriter, bDoChecksum, ArrayHandleIterator, NewData );
			}

			uint32 EndArrayIndex = 0;
			TempWriter.SerializeIntPacked( EndArrayIndex );

			uint32 NumBits = TempWriter.GetNumBits();
			Writer.SerializeIntPacked( NumBits );
			Writer.SerializeBits( TempWriter.GetData(), NumBits );

			check( HandleIterator.ChangelistIterator.ChangedIndex - OldChangedIndex == ArrayChangedCount );				// Make sure we read correct amount
			check( HandleIterator.ChangelistIterator.Changed[HandleIterator.ChangelistIterator.ChangedIndex] == 0 );	// Make sure we are at the end

			HandleIterator.ChangelistIterator.ChangedIndex++;
			continue;
		}

		WriteProperty_BackwardsCompatible( Writer, Cmd, HandleIterator.CmdIndex, Owner, Data, bDoChecksum );
	}

	WritePropertyHandle_BackwardsCompatible( Writer, 0, bDoChecksum );
}

void FRepLayout::SendAllProperties_BackwardsCompatible_r( 
	FNetBitWriter&						Writer,
	const bool							bDoChecksum,
	UPackageMapClient*					PackageMapClient,
	FNetFieldExportGroup*				NetFieldExportGroup,
	const int32							CmdStart,
	const int32							CmdEnd, 
	const uint8*						SourceData ) const
{
	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		check( Cmd.Type != REPCMD_Return );

		PackageMapClient->TrackNetFieldExport( NetFieldExportGroup, CmdIndex );

		WritePropertyHandle_BackwardsCompatible( Writer, CmdIndex + 1, bDoChecksum );

		const uint8* Data = SourceData + Cmd.Offset;

		if ( Cmd.Type == REPCMD_DynamicArray )
		{			
			const FScriptArray* Array = ( FScriptArray * )Data;

			FNetBitWriter TempWriter( Writer.PackageMap, 0 );

			// Write array num
			uint32 ArrayNum = Array->Num();
			TempWriter.SerializeIntPacked( ArrayNum );

			for ( int32 i = 0; i < Array->Num(); i++ )
			{
				uint32 ArrayIndex = i + 1;
				TempWriter.SerializeIntPacked( ArrayIndex );

				SendAllProperties_BackwardsCompatible_r( TempWriter, bDoChecksum, PackageMapClient, NetFieldExportGroup, CmdIndex + 1, Cmd.EndCmd - 1, ((const uint8*)Array->GetData()) + Cmd.ElementSize * i );
			}

			uint32 EndArrayIndex = 0;
			TempWriter.SerializeIntPacked( EndArrayIndex );

			uint32 NumBits = TempWriter.GetNumBits();
			Writer.SerializeIntPacked( NumBits );
			Writer.SerializeBits( TempWriter.GetData(), NumBits );

			CmdIndex = Cmd.EndCmd - 1;		// The -1 to handle the ++ in the for loop
			continue;
		}

		WriteProperty_BackwardsCompatible( Writer, Cmd, CmdIndex, Owner, Data, bDoChecksum );
	}

	WritePropertyHandle_BackwardsCompatible( Writer, 0, bDoChecksum );
}

void FRepLayout::SendProperties_BackwardsCompatible(
	FRepState* RESTRICT			RepState,
	FRepChangedPropertyTracker* ChangedTracker,
	const uint8* RESTRICT		Data,
	UNetConnection*				Connection,
	FNetBitWriter&				Writer,
	TArray< uint16 >&			Changed ) const
{
	FBitWriterMark Mark( Writer );

#ifdef ENABLE_PROPERTY_CHECKSUMS
	const bool bDoChecksum = CVarDoPropertyChecksum.GetValueOnAnyThread() == 1;
	Writer.WriteBit( bDoChecksum ? 1 : 0 );
#else
	const bool bDoChecksum = false;
#endif

	UPackageMapClient* PackageMapClient = ( UPackageMapClient* )Connection->PackageMap;

	TSharedPtr< FNetFieldExportGroup > NetFieldExportGroup = PackageMapClient->GetNetFieldExportGroup( Owner->GetPathName() );

	if ( !NetFieldExportGroup.IsValid() )
	{
		NetFieldExportGroup = CreateNetfieldExportGroup();

		PackageMapClient->AddNetFieldExportGroup( Owner->GetPathName(), NetFieldExportGroup );
	}

	const int32 NumBits = Writer.GetNumBits();

	if ( Changed.Num() == 0 )
	{
		SendAllProperties_BackwardsCompatible_r( Writer, bDoChecksum, PackageMapClient, NetFieldExportGroup.Get(), 0, Cmds.Num() - 1, Data );
	}
	else
	{
		FChangelistIterator ChangelistIterator( Changed, 0 );
		FRepHandleIterator HandleIterator( ChangelistIterator, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1 );

		SendProperties_BackwardsCompatible_r( RepState, PackageMapClient, NetFieldExportGroup.Get(), ChangedTracker, Writer, bDoChecksum, HandleIterator, Data );
	}

	if ( NumBits == Writer.GetNumBits() )
	{
		Mark.Pop( Writer );
	}
}

class FReceivedPropertiesStackState : public FCmdIteratorBaseStackState
{
public:
	FReceivedPropertiesStackState( const int32 InCmdStart, const int32 InCmdEnd, FScriptArray*	InShadowArray, FScriptArray* InDataArray, uint8* RESTRICT InShadowBaseData, uint8* RESTRICT	InBaseData ) : 
		FCmdIteratorBaseStackState( InCmdStart, InCmdEnd, InShadowArray, InDataArray, InShadowBaseData, InBaseData ),
		GuidReferencesMap( NULL )
	{}

	FGuidReferencesMap* GuidReferencesMap;
};

static bool ReceivePropertyHelper( 
	FNetBitReader&					Bunch, 
	FGuidReferencesMap*				GuidReferencesMap,
	const int32						ElementOffset, 
	uint8* RESTRICT					ShadowData,
	uint8* RESTRICT					Data,
	TArray< UProperty * >*			RepNotifies,
	const TArray< FRepParentCmd >&	Parents,
	const TArray< FRepLayoutCmd >&	Cmds,
	const int32						CmdIndex,
	const bool						bDoChecksum,
	bool&							bOutGuidsChanged )
{
	const FRepLayoutCmd& Cmd	= Cmds[CmdIndex];
	const FRepParentCmd& Parent = Parents[Cmd.ParentIndex];

	// This swaps Role/RemoteRole as we write it
	const FRepLayoutCmd& SwappedCmd = Parent.RoleSwapIndex != -1 ? Cmds[Parents[Parent.RoleSwapIndex].CmdStart] : Cmd;

	if ( GuidReferencesMap )		// Don't reset unmapped guids here if we are told not to (assuming calling code is handling this)
	{
		// Let package map know we want to track and know about any guids that are unmapped during the serialize call
		Bunch.PackageMap->ResetTrackedGuids( true );
	}

	// Remember where we started reading from, so that if we have unmapped properties, we can re-deserialize from this data later
	FBitReaderMark Mark( Bunch );

	if ( RepNotifies != nullptr && Parent.Property->HasAnyPropertyFlags( CPF_RepNotify ) )
	{
		// Copy current value over so we can check to see if it changed
		StoreProperty( Cmd, ShadowData + Cmd.Offset, Data + SwappedCmd.Offset );

		// Read the property
		Cmd.Property->NetSerializeItem( Bunch, Bunch.PackageMap, Data + SwappedCmd.Offset );

		// Check to see if this property changed
		if ( Parent.RepNotifyCondition == REPNOTIFY_Always || !PropertiesAreIdentical( Cmd, ShadowData + Cmd.Offset, Data + SwappedCmd.Offset ) )
		{
			(*RepNotifies).AddUnique( Parent.Property );
		}
		else
		{
			UE_CLOG( LogSkippedRepNotifies > 0, LogRep, Display, TEXT( "2 FReceivedPropertiesStackState Skipping RepNotify for propery %s because local value has not changed." ), *Cmd.Property->GetName() );
		}
	}
	else
	{
		Cmd.Property->NetSerializeItem( Bunch, Bunch.PackageMap, Data + SwappedCmd.Offset );
	}

#ifdef ENABLE_PROPERTY_CHECKSUMS
	if ( bDoChecksum )
	{
		SerializeReadWritePropertyChecksum( Cmd, CmdIndex, Data + SwappedCmd.Offset, Bunch );
	}
#endif

	if ( GuidReferencesMap )
	{
		const int32 AbsOffset = ElementOffset + SwappedCmd.Offset;

		// Loop over all de-serialized network guids and track them so we can manage their pointers as their replicated reference goes in/out of relevancy
		const TSet< FNetworkGUID >& TrackedUnmappedGuids = Bunch.PackageMap->GetTrackedUnmappedGuids();
		const TSet< FNetworkGUID >& TrackedDynamicMappedGuids = Bunch.PackageMap->GetTrackedDynamicMappedGuids();

		const bool bHasUnmapped = TrackedUnmappedGuids.Num() > 0;

		FGuidReferences* GuidReferences = GuidReferencesMap->Find( AbsOffset );

		if ( TrackedUnmappedGuids.Num() > 0 || TrackedDynamicMappedGuids.Num() > 0 )
		{
			if ( GuidReferences != nullptr )
			{
				check( GuidReferences->CmdIndex == CmdIndex );
				check( GuidReferences->ParentIndex == Cmd.ParentIndex );

				// If we're already tracking the guids, re-copy lists only if they've changed
				if ( !NetworkGuidSetsAreSame( GuidReferences->UnmappedGUIDs, TrackedUnmappedGuids ) )
				{
					bOutGuidsChanged = true;
				}
				else if ( !NetworkGuidSetsAreSame( GuidReferences->MappedDynamicGUIDs, TrackedDynamicMappedGuids ) )
				{
					bOutGuidsChanged = true;
				}
			}
			
			if ( GuidReferences == nullptr || bOutGuidsChanged )
			{
				// First time tracking these guids (or guids changed), so add (or replace) new entry
				GuidReferencesMap->Add( AbsOffset, FGuidReferences( Bunch, Mark, TrackedUnmappedGuids, TrackedDynamicMappedGuids, Cmd.ParentIndex, CmdIndex ) );
				bOutGuidsChanged = true;
			}
		}
		else
		{
			// If we don't have any unmapped guids, then make sure to remove the entry so we don't serialize old data when we update unmapped objects
			if ( GuidReferences != nullptr )
			{
				GuidReferencesMap->Remove( AbsOffset );
				bOutGuidsChanged = true;
			}
		}

		// Stop tracking unmapped objects
		Bunch.PackageMap->ResetTrackedGuids( false );

		return bHasUnmapped;
	}

	return false;
}

static FGuidReferencesMap* PrepReceivedArray(
	const int32				ArrayNum,
	FScriptArray*			ShadowArray,
	FScriptArray*			DataArray,
	FGuidReferencesMap*		ParentGuidReferences,
	const int32				AbsOffset,
	const FRepParentCmd&	Parent, 
	const FRepLayoutCmd&	Cmd, 
	const int32				CmdIndex,
	uint8* RESTRICT*		OutShadowBaseData,
	uint8* RESTRICT*		OutBaseData,
	TArray< UProperty * >*	RepNotifies )
{
	FGuidReferences* NewGuidReferencesArray = nullptr;

	if ( ParentGuidReferences != nullptr )
	{
		// Since we don't know yet if something under us could be unmapped, go ahead and allocate an array container now
		NewGuidReferencesArray = ParentGuidReferences->Find( AbsOffset );

		if ( NewGuidReferencesArray == NULL )
		{
			NewGuidReferencesArray = &ParentGuidReferences->FindOrAdd( AbsOffset );

			NewGuidReferencesArray->Array		= new FGuidReferencesMap;
			NewGuidReferencesArray->ParentIndex	= Cmd.ParentIndex;
			NewGuidReferencesArray->CmdIndex	= CmdIndex;
		}

		check( NewGuidReferencesArray != NULL );
		check( NewGuidReferencesArray->ParentIndex == Cmd.ParentIndex );
		check( NewGuidReferencesArray->CmdIndex == CmdIndex );
	}

	if ( RepNotifies != nullptr )
	{
		if ( ( DataArray->Num() != ArrayNum || Parent.RepNotifyCondition == REPNOTIFY_Always ) && Parent.Property->HasAnyPropertyFlags( CPF_RepNotify ) )
		{
			( *RepNotifies ).AddUnique( Parent.Property );
		}
		else
		{
			UE_CLOG( LogSkippedRepNotifies > 0, LogRep, Display, TEXT( "1 FReceivedPropertiesStackState Skipping RepNotify for propery %s because local value has not changed." ), *Cmd.Property->GetName() );
		}
	}

	check( CastChecked< UArrayProperty >( Cmd.Property ) != NULL );

	// Resize arrays if needed
	FScriptArrayHelper ArrayHelper( ( UArrayProperty * )Cmd.Property, DataArray );
	ArrayHelper.Resize( ArrayNum );

	// Re-compute the base data values since they could have changed after the resize above
	*OutBaseData		= ( uint8* )DataArray->GetData();
	*OutShadowBaseData	= nullptr;

	// Only resize the shadow data array if we're actually tracking RepNotifies
	if ( RepNotifies != nullptr )
	{
		check( ShadowArray != nullptr );

		FScriptArrayHelper ShadowArrayHelper( ( UArrayProperty* )Cmd.Property, ShadowArray );
		ShadowArrayHelper.Resize( ArrayNum );

		*OutShadowBaseData = ( uint8* )ShadowArray->GetData();
	}

	return NewGuidReferencesArray ? NewGuidReferencesArray->Array : nullptr;
}

class FReceivePropertiesImpl : public FRepLayoutCmdIterator< FReceivePropertiesImpl, FReceivedPropertiesStackState >
{
public:
	FReceivePropertiesImpl( FNetBitReader & InBunch, FRepState * InRepState, bool bInDoChecksum, const TArray< FRepParentCmd >& InParents, const TArray< FRepLayoutCmd >& InCmds, const bool bInDoRepNotify ) :
        FRepLayoutCmdIterator( InParents, InCmds ),
		WaitingHandle( 0 ),
		CurrentHandle( 0 ), 
		Bunch( InBunch ),
		RepState( InRepState ),
		bDoChecksum( bInDoChecksum ),
		bHasUnmapped( false ),
		bDoRepNotify( bInDoRepNotify )
	{}

	void ReadNextHandle()
	{
		Bunch.SerializeIntPacked( WaitingHandle );

#ifdef ENABLE_PROPERTY_CHECKSUMS
		if ( bDoChecksum )
		{
			SerializeGenericChecksum( Bunch );
		}
#endif
	}

	INIT_STACK( FReceivedPropertiesStackState )
	{
		StackState.GuidReferencesMap = &RepState->GuidReferencesMap;
	}

	SHOULD_PROCESS_NEXT_CMD()
	{
		CurrentHandle++;

		if ( CurrentHandle == WaitingHandle )
		{
			check( WaitingHandle != 0 );
			return true;
		}

		return false;
	}

	PROCESS_ARRAY_CMD( FReceivedPropertiesStackState )
	{
		// Read array size
		uint16 ArrayNum = 0;
		Bunch << ArrayNum;

		// Read the next property handle
		ReadNextHandle();

		const int32 AbsOffset = Data - PrevStackState.BaseData;

		const FRepParentCmd& Parent = Parents[Cmd.ParentIndex];

		StackState.GuidReferencesMap = PrepReceivedArray(
			ArrayNum,
			StackState.ShadowArray,
			StackState.DataArray,
			PrevStackState.GuidReferencesMap,
			AbsOffset,
			Parent,
			Cmd,
			CmdIndex,
			&StackState.ShadowBaseData,
			&StackState.BaseData,
			bDoRepNotify ? &RepState->RepNotifies : nullptr );

		// Save the old handle so we can restore it when we pop out of the array
		const uint16 OldHandle = CurrentHandle;

		// Array children handles are always relative to their immediate parent
		CurrentHandle = 0;

		// Loop over array
		ProcessDataArrayElements_r( StackState, Cmd );

		// Restore the current handle to what it was before we processed this array
		CurrentHandle = OldHandle;

		// We should be waiting on the NULL terminator handle at this point
		check( WaitingHandle == 0 );
		ReadNextHandle();
	}

	PROCESS_CMD( FReceivedPropertiesStackState )
	{
		check( StackState.GuidReferencesMap != NULL );

		const int32 ElementOffset = ( Data - StackState.BaseData );

		if ( ReceivePropertyHelper( Bunch, StackState.GuidReferencesMap, ElementOffset, ShadowData, Data, bDoRepNotify ? &RepState->RepNotifies : nullptr, Parents, Cmds, CmdIndex, bDoChecksum, bGuidsChanged ) )
		{
			bHasUnmapped = true;
		}

		// Read the next property handle
		ReadNextHandle();
	}

	uint32					WaitingHandle;
	uint32					CurrentHandle;
	FNetBitReader &			Bunch;
	FRepState *				RepState;
	bool					bDoChecksum;
	bool					bHasUnmapped;
	bool					bDoRepNotify;
	bool					bGuidsChanged;
};

bool FRepLayout::ReceiveProperties( UActorChannel* OwningChannel, UClass * InObjectClass, FRepState * RESTRICT RepState, void* RESTRICT Data, FNetBitReader & InBunch, bool & bOutHasUnmapped, const bool bEnableRepNotifies, bool& bOutGuidsChanged ) const
{
	check( InObjectClass == Owner );

#ifdef ENABLE_PROPERTY_CHECKSUMS
	const bool bDoChecksum = InBunch.ReadBit() ? true : false;
#else
	const bool bDoChecksum = false;
#endif

	bOutHasUnmapped = false;

	if ( OwningChannel->Connection->InternalAck )
	{
		TSharedPtr< FNetFieldExportGroup > NetFieldExportGroup = ( ( UPackageMapClient* )OwningChannel->Connection->PackageMap )->GetNetFieldExportGroup( Owner->GetPathName() );

		if ( !ensure( NetFieldExportGroup.IsValid() ) )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible: Invalid path name: %s" ), *Owner->GetPathName() );
			InBunch.SetError();
			return false;
		}

		return ReceiveProperties_BackwardsCompatible_r( RepState, NetFieldExportGroup.Get(), InBunch, 0, Cmds.Num() - 1, bEnableRepNotifies ? RepState->StaticBuffer.GetData() : nullptr, ( uint8* )Data, ( uint8* )Data, &RepState->GuidReferencesMap, bOutHasUnmapped, bOutGuidsChanged );
	}

	FReceivePropertiesImpl ReceivePropertiesImpl( InBunch, RepState, bDoChecksum, Parents, Cmds, bEnableRepNotifies );

	// Read first handle
	ReceivePropertiesImpl.ReadNextHandle();

	// Read all properties
	ReceivePropertiesImpl.ProcessCmds( (uint8*)Data, RepState->StaticBuffer.GetData() );

	// Make sure we're waiting on the last NULL terminator
	if ( ReceivePropertiesImpl.WaitingHandle != 0 )
	{
		UE_LOG( LogRep, Warning, TEXT( "Read out of sync." ) );
		return false;
	}

#ifdef ENABLE_SUPER_CHECKSUMS
	if ( InBunch.ReadBit() == 1 )
	{
		ValidateWithChecksum( RepState->StaticBuffer.GetData(), InBunch );
	}
#endif

	bOutHasUnmapped = ReceivePropertiesImpl.bHasUnmapped;
	bOutGuidsChanged = ReceivePropertiesImpl.bGuidsChanged;

	return true;
}

bool FRepLayout::ReceiveProperties_BackwardsCompatible( UNetConnection* Connection, FRepState* RESTRICT RepState, void* RESTRICT Data, FNetBitReader& InBunch, bool& bOutHasUnmapped, const bool bEnableRepNotifies, bool& bOutGuidsChanged ) const
{
#ifdef ENABLE_PROPERTY_CHECKSUMS
	const bool bDoChecksum = InBunch.ReadBit() ? true : false;
#else
	const bool bDoChecksum = false;
#endif

	bOutHasUnmapped = false;

	TSharedPtr< FNetFieldExportGroup > NetFieldExportGroup = ( ( UPackageMapClient* )Connection->PackageMap )->GetNetFieldExportGroup( Owner->GetPathName() );

	return ReceiveProperties_BackwardsCompatible_r( RepState, NetFieldExportGroup.Get(), InBunch, 0, Cmds.Num() - 1, (bEnableRepNotifies && RepState) ? RepState->StaticBuffer.GetData() : nullptr, ( uint8* )Data, ( uint8* )Data, RepState ? &RepState->GuidReferencesMap : nullptr, bOutHasUnmapped, bOutGuidsChanged );
}

int32 FRepLayout::FindCompatibleProperty( const int32 CmdStart, const int32 CmdEnd, const uint32 Checksum ) const
{
	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		check( Cmd.Type != REPCMD_Return );

		if ( Cmd.CompatibleChecksum == Checksum )
		{
			return CmdIndex;
		}

		// Jump over entire array and inner properties if checksum didn't match
		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			CmdIndex = Cmd.EndCmd - 1;
		}
	}

	return -1;
}

bool FRepLayout::ReceiveProperties_BackwardsCompatible_r( 
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
	bool&					bOutGuidsChanged ) const
{
	while ( true )
	{
		uint32 NetFieldExportHandle = 0;
		Reader.SerializeIntPacked( NetFieldExportHandle );

		if ( Reader.IsError() )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Error reading handle. Owner: %s" ), *Owner->GetName() );
			return false;
		}

		if ( NetFieldExportHandle == 0 )
		{
			// We're done
			break;
		}

		if ( !ensure( NetFieldExportGroup != nullptr ) )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: NetFieldExportGroup == nullptr. Owner: %s, NetFieldExportHandle: %u" ), *Owner->GetName(), NetFieldExportHandle );
			Reader.SetError();
			return false;
		}

		// We purposely add 1 on save, so we can reserve 0 for "done"
		NetFieldExportHandle--;

		if ( !ensure( NetFieldExportHandle < ( uint32 )NetFieldExportGroup->NetFieldExports.Num() ) )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: NetFieldExportHandle > NetFieldExportGroup->NetFieldExports.Num(). Owner: %s, NetFieldExportHandle: %u" ), *Owner->GetName(), NetFieldExportHandle );
			return false;
		}

		const uint32 Checksum = NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].CompatibleChecksum;

		if ( !ensure( Checksum != 0 ) )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Checksum == 0. Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i" ), *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle );
			return false;
		}

		uint32 NumBits = 0;
		Reader.SerializeIntPacked( NumBits );

		if ( Reader.IsError() )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Error reading num bits. Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );
			return false;
		}

		FNetBitReader TempReader;
		
		TempReader.PackageMap = Reader.PackageMap;
		TempReader.SetData( Reader, NumBits );

		if ( Reader.IsError() )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Error reading payload. Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );
			return false;
		}

		if ( NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].bIncompatible )
		{
			continue;		// We've already warned that this property doesn't load anymore
		}

		// Find this property
		const int32 CmdIndex = FindCompatibleProperty( CmdStart, CmdEnd, Checksum );

		if ( CmdIndex == -1 )
		{
			UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Property not found. Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );

			// Mark this property as incompatible so we don't keep spamming this warning
			NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].bIncompatible = true;
			continue;
		}

		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			uint32 ArrayNum = 0;
			TempReader.SerializeIntPacked( ArrayNum );

			if ( TempReader.IsError() )
			{
				return false;
			}

			const int32 AbsOffset = ( Data - OldData ) + Cmd.Offset;

			FScriptArray* DataArray		= ( FScriptArray* )( Data + Cmd.Offset );
			FScriptArray* ShadowArray	= ShadowData ? ( FScriptArray* )( ShadowData + Cmd.Offset ) : nullptr;

			uint8* LocalData			= Data;
			uint8* LocalShadowData		= ShadowData;

			FGuidReferencesMap* NewGuidReferencesArray = PrepReceivedArray(
				ArrayNum,
				ShadowArray,
				DataArray,
				GuidReferencesMap,
				AbsOffset,
				Parents[Cmd.ParentIndex],
				Cmd,
				CmdIndex,
				&LocalShadowData,
				&LocalData,
				ShadowData != nullptr ? &RepState->RepNotifies : nullptr );

			// Read until we read all array elements
			while ( true )
			{
				uint32 Index = 0;
				TempReader.SerializeIntPacked( Index );

				if ( TempReader.IsError() )
				{
					UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Error reading array index. Index: %i, Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), Index, *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );
					return false;
				}

				if ( Index == 0 )
				{
					// We're done
					break;
				}

				// Shift all indexes down since 0 represents null handle
				Index--;

				if ( !ensure( Index < ArrayNum ) )
				{
					UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Array index out of bounds. Index: %i, ArrayNum: %i, Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), Index, ArrayNum, *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );
					return false;
				}

				const int32 ElementOffset = Index * Cmd.ElementSize;

				uint8* ElementData			= LocalData + ElementOffset;
				uint8* ElementShadowData	= LocalShadowData ? LocalShadowData + ElementOffset : nullptr;

				if ( !ReceiveProperties_BackwardsCompatible_r( RepState, NetFieldExportGroup, TempReader, CmdIndex + 1, Cmd.EndCmd - 1, ElementShadowData, LocalData, ElementData, NewGuidReferencesArray, bOutHasUnmapped, bOutGuidsChanged ) )
				{
					return false;
				}

				if ( TempReader.IsError() )
				{
					UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Error reading array index element payload. Index: %i, Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), Index, *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );
					return false;
				}
			}

			if ( TempReader.GetBitsLeft() != 0 )
			{
				UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Array didn't read propery number of bits. Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );
				return false;
			}
		}
		else
		{
			const int32 ElementOffset = ( Data - OldData );

			if ( ReceivePropertyHelper( TempReader, GuidReferencesMap, ElementOffset, ShadowData, Data, ShadowData != nullptr ? &RepState->RepNotifies : nullptr, Parents, Cmds, CmdIndex, false, bOutGuidsChanged ) )
			{
				bOutHasUnmapped = true;
			}

			if ( TempReader.GetBitsLeft() != 0 )
			{
				UE_LOG( LogRep, Warning, TEXT( "ReceiveProperties_BackwardsCompatible_r: Property didn't read propery number of bits. Owner: %s, Name: %s, Type: %s, NetFieldExportHandle: %i, Checksum: %u" ), *Owner->GetName(), *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Name, *NetFieldExportGroup->NetFieldExports[NetFieldExportHandle].Type, NetFieldExportHandle, Checksum );
				return false;
			}
		}
	}

	return true;
}

FGuidReferences::~FGuidReferences()
{
	if ( Array != NULL )
	{
		delete Array;
		Array = NULL;
	}
}

void FRepLayout::GatherGuidReferences_r( FGuidReferencesMap* GuidReferencesMap, TSet< FNetworkGUID >& OutReferencedGuids, int32& OutTrackedGuidMemoryBytes ) const
{
	for ( const auto& GuidReferencePair : *GuidReferencesMap )
	{
		const FGuidReferences& GuidReferences = GuidReferencePair.Value;

		if ( GuidReferences.Array != NULL )
		{
			check( Cmds[GuidReferences.CmdIndex].Type == REPCMD_DynamicArray );

			GatherGuidReferences_r( GuidReferences.Array, OutReferencedGuids, OutTrackedGuidMemoryBytes );
			continue;
		}

		OutTrackedGuidMemoryBytes += GuidReferences.Buffer.Num();

		OutReferencedGuids.Append( GuidReferences.UnmappedGUIDs );
		OutReferencedGuids.Append( GuidReferences.MappedDynamicGUIDs );
	}
}

void FRepLayout::GatherGuidReferences( FRepState* RepState, TSet< FNetworkGUID >& OutReferencedGuids, int32& OutTrackedGuidMemoryBytes ) const
{
	GatherGuidReferences_r( &RepState->GuidReferencesMap, OutReferencedGuids, OutTrackedGuidMemoryBytes );
}

bool FRepLayout::MoveMappedObjectToUnmapped_r( FGuidReferencesMap* GuidReferencesMap, const FNetworkGUID& GUID ) const
{
	bool bFoundGUID = false;

	for ( auto& GuidReferencePair : *GuidReferencesMap )
	{
		FGuidReferences& GuidReferences = GuidReferencePair.Value;

		if ( GuidReferences.Array != NULL )
		{
			check( Cmds[GuidReferences.CmdIndex].Type == REPCMD_DynamicArray );

			if ( MoveMappedObjectToUnmapped_r( GuidReferences.Array, GUID ) )
			{
				bFoundGUID = true;
			}
			continue;
		}

		if ( GuidReferences.MappedDynamicGUIDs.Contains( GUID ) )
		{
			GuidReferences.MappedDynamicGUIDs.Remove( GUID );
			GuidReferences.UnmappedGUIDs.Add( GUID );
			bFoundGUID = true;
		}
	}

	return bFoundGUID;
}

bool FRepLayout::MoveMappedObjectToUnmapped( FRepState* RepState, const FNetworkGUID& GUID ) const
{
	return MoveMappedObjectToUnmapped_r( &RepState->GuidReferencesMap, GUID );
}

void FRepLayout::UpdateUnmappedObjects_r( 
	FRepState*				RepState, 
	FGuidReferencesMap*		GuidReferencesMap,
	UObject*				OriginalObject,
	UPackageMap*			PackageMap, 
	uint8* RESTRICT			StoredData, 
	uint8* RESTRICT			Data, 
	const int32				MaxAbsOffset,
	bool&					bOutSomeObjectsWereMapped,
	bool&					bOutHasMoreUnmapped ) const
{
	for ( auto It = GuidReferencesMap->CreateIterator(); It; ++It )
	{
		const int32 AbsOffset = It.Key();

		if ( AbsOffset >= MaxAbsOffset )
		{
			// Array must have shrunk, we can remove this item
			UE_LOG( LogRep, VeryVerbose, TEXT( "UpdateUnmappedObjects_r: REMOVED unmapped property: AbsOffset >= MaxAbsOffset. Offset: %i" ), AbsOffset );
			It.RemoveCurrent();
			continue;
		}

		FGuidReferences&			GuidReferences = It.Value();
		const FRepLayoutCmd&		Cmd = Cmds[GuidReferences.CmdIndex];
		const FRepParentCmd&		Parent = Parents[GuidReferences.ParentIndex];

		if ( GuidReferences.Array != NULL )
		{
			check( Cmd.Type == REPCMD_DynamicArray );
			
			FScriptArray* StoredArray = (FScriptArray *)( StoredData + AbsOffset );
			FScriptArray* Array = (FScriptArray *)( Data + AbsOffset );
			
			const int32 NewMaxOffset = FMath::Min( StoredArray->Num() * Cmd.ElementSize, Array->Num() * Cmd.ElementSize );

			UpdateUnmappedObjects_r( RepState, GuidReferences.Array, OriginalObject, PackageMap, (uint8*)StoredArray->GetData(), (uint8*)Array->GetData(), NewMaxOffset, bOutSomeObjectsWereMapped, bOutHasMoreUnmapped );
			continue;
		}

		bool bMappedSomeGUIDs = false;

		for ( auto UnmappedIt = GuidReferences.UnmappedGUIDs.CreateIterator(); UnmappedIt; ++UnmappedIt )
		{			
			const FNetworkGUID& GUID = *UnmappedIt;

			if ( PackageMap->IsGUIDBroken( GUID, false ) )
			{
				UE_LOG( LogRep, Warning, TEXT( "UpdateUnmappedObjects_r: Broken GUID. NetGuid: %s" ), *GUID.ToString() );
				UnmappedIt.RemoveCurrent();
				continue;
			}

			UObject* Object = PackageMap->GetObjectFromNetGUID( GUID, false );

			if ( Object != NULL )
			{
				UE_LOG( LogRep, VeryVerbose, TEXT( "UpdateUnmappedObjects_r: REMOVED unmapped property: Offset: %i, Guid: %s, PropName: %s, ObjName: %s" ), AbsOffset, *GUID.ToString(), *Cmd.Property->GetName(), *Object->GetName() );

				if ( GUID.IsDynamic() )
				{
					// If this guid is dynamic, move it to the dynamic guids list
					GuidReferences.MappedDynamicGUIDs.Add( GUID );
				}

				// Remove from unmapped guids list
				UnmappedIt.RemoveCurrent();
				bMappedSomeGUIDs = true;
			}
		}

		// If we resolved some guids, re-deserialize the data which will hook up the object pointer with the property
		if ( bMappedSomeGUIDs )
		{
			if ( !bOutSomeObjectsWereMapped )
			{
				// Call PreNetReceive if we are going to change a value (some game code will need to think this is an actual replicated value)
				OriginalObject->PreNetReceive();
				bOutSomeObjectsWereMapped = true;
			}

			// Copy current value over so we can check to see if it changed
			if ( Parent.Property->HasAnyPropertyFlags( CPF_RepNotify ) )
			{
				StoreProperty( Cmd, StoredData + AbsOffset, Data + AbsOffset );
			}

			// Initialize the reader with the stored buffer that we need to read from
			FNetBitReader Reader( PackageMap, GuidReferences.Buffer.GetData(), GuidReferences.NumBufferBits );

			// Read the property
			Cmd.Property->NetSerializeItem( Reader, PackageMap, Data + AbsOffset );

			// Check to see if this property changed
			if ( Parent.Property->HasAnyPropertyFlags( CPF_RepNotify ) )
			{
				if ( Parent.RepNotifyCondition == REPNOTIFY_Always || !PropertiesAreIdentical( Cmd, StoredData + AbsOffset, Data + AbsOffset ) )
				{
					// If this properties needs an OnRep, queue that up to be handled later
					RepState->RepNotifies.AddUnique( Parent.Property );
				}
				else
				{
					UE_CLOG( LogSkippedRepNotifies, LogRep, Display, TEXT( "UpdateUnmappedObjects_r: Skipping RepNotify because Property did not change. %s" ), *Cmd.Property->GetName() );
				}
			}
		}

		// If we still have more unmapped guids, we need to keep processing this entry
		if ( GuidReferences.UnmappedGUIDs.Num() > 0 )
		{
			bOutHasMoreUnmapped = true;
		}
		else if ( GuidReferences.UnmappedGUIDs.Num() == 0 && GuidReferences.MappedDynamicGUIDs.Num() == 0 )
		{
			It.RemoveCurrent();
		}
	}
}

void FRepLayout::UpdateUnmappedObjects( FRepState *	RepState, UPackageMap * PackageMap, UObject* OriginalObject, bool & bOutSomeObjectsWereMapped, bool & bOutHasMoreUnmapped ) const
{
	bOutSomeObjectsWereMapped	= false;
	bOutHasMoreUnmapped			= false;

	UpdateUnmappedObjects_r( RepState, &RepState->GuidReferencesMap, OriginalObject, PackageMap, (uint8*)RepState->StaticBuffer.GetData(), (uint8*)OriginalObject, RepState->StaticBuffer.Num(), bOutSomeObjectsWereMapped, bOutHasMoreUnmapped );
}

void FRepLayout::CallRepNotifies( FRepState * RepState, UObject* Object ) const
{
	if ( RepState->RepNotifies.Num() == 0 )
	{
		return;
	}

	for ( int32 i = 0; i < RepState->RepNotifies.Num(); i++ )
	{
		UProperty * RepProperty = RepState->RepNotifies[i];

		UFunction * RepNotifyFunc = Object->FindFunction( RepProperty->RepNotifyFunc );

		if (RepNotifyFunc == nullptr)
		{
			UE_LOG(LogRep, Warning, TEXT("FRepLayout::CallRepNotifies: Can't find RepNotify function %s for property %s on object %s."),
				*RepProperty->RepNotifyFunc.ToString(), *RepProperty->GetName(), *Object->GetName());
			continue;
		}

		check( RepNotifyFunc->NumParms <= 1 );	// 2 parms not supported yet

		if ( RepNotifyFunc->NumParms == 0 )
		{
			Object->ProcessEvent( RepNotifyFunc, NULL );
		}
		else if (RepNotifyFunc->NumParms == 1 )
		{
			Object->ProcessEvent( RepNotifyFunc, RepProperty->ContainerPtrToValuePtr<uint8>( RepState->StaticBuffer.GetData() ) );
		}

		// Store the property we just received
		//StoreProperty( Cmd, StoredData + Cmd.Offset, Data + SwappedCmd.Offset );
	}

	RepState->RepNotifies.Empty();
}

void FRepLayout::ValidateWithChecksum_DynamicArray_r( const FRepLayoutCmd& Cmd, const int32 CmdIndex, const uint8* RESTRICT Data, FArchive & Ar ) const
{
	FScriptArray * Array = (FScriptArray *)Data;

	uint16 ArrayNum		= Array->Num();
	uint16 ElementSize	= Cmd.ElementSize;

	Ar << ArrayNum;
	Ar << ElementSize;

	if ( ArrayNum != Array->Num() )
	{
		UE_LOG( LogRep, Fatal, TEXT( "ValidateWithChecksum_AnyArray_r: Array sizes different! %s %i / %i" ), *Cmd.Property->GetFullName(), ArrayNum, Array->Num() );
	}

	if ( ElementSize != Cmd.ElementSize )
	{
		UE_LOG( LogRep, Fatal, TEXT( "ValidateWithChecksum_AnyArray_r: Array element sizes different! %s %i / %i" ), *Cmd.Property->GetFullName(), ElementSize, Cmd.ElementSize );
	}

	uint8* LocalData = (uint8*)Array->GetData();

	for ( int32 i = 0; i < ArrayNum; i++ )
	{
		ValidateWithChecksum_r( CmdIndex + 1, Cmd.EndCmd - 1, LocalData + i * ElementSize, Ar );
	}
}

void FRepLayout::ValidateWithChecksum_r( 
	const int32				CmdStart, 
	const int32				CmdEnd, 
	const uint8* RESTRICT	Data, 
	FArchive &				Ar ) const
{
	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[ CmdIndex ];

		check( Cmd.Type != REPCMD_Return );

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			ValidateWithChecksum_DynamicArray_r( Cmd, CmdIndex, Data + Cmd.Offset, Ar );
			CmdIndex = Cmd.EndCmd - 1;	// Jump past children of this array (-1 for ++ in for loop)
			continue;
		}

		SerializeReadWritePropertyChecksum( Cmd, CmdIndex - 1, Data + Cmd.Offset, Ar );
	}
}

void FRepLayout::ValidateWithChecksum( const void* RESTRICT Data, FArchive & Ar ) const
{
	ValidateWithChecksum_r( 0, Cmds.Num() - 1, (const uint8*)Data, Ar );
}

uint32 FRepLayout::GenerateChecksum( const FRepState* RepState ) const
{
	FBitWriter Writer( 1024, true );
	ValidateWithChecksum_r( 0, Cmds.Num() - 1, (const uint8*)RepState->StaticBuffer.GetData(), Writer );

	return FCrc::MemCrc32( Writer.GetData(), Writer.GetNumBytes(), 0 );
}

void FRepLayout::PruneChangeList( FRepState* RepState, const void* RESTRICT Data, const TArray< uint16 >& Changed, TArray< uint16 >& PrunedChanged ) const
{
	check( Changed.Num() > 0 );

	PrunedChanged.Empty();

	FChangelistIterator ChangelistIterator( Changed, 0 );
	FRepHandleIterator HandleIterator( ChangelistIterator, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1 );
	PruneChangeList_r( HandleIterator, ( uint8* )Data, PrunedChanged );

	PrunedChanged.Add( 0 );
}

void FRepLayout::MergeChangeList( const uint8* RESTRICT Data, const TArray< uint16 > & Dirty1, const TArray< uint16 > & Dirty2, TArray< uint16 > & MergedDirty ) const
{
	check( Dirty1.Num() > 0 );

	MergedDirty.Empty();

	if ( Dirty2.Num() == 0 )
	{
		FChangelistIterator ChangelistIterator( Dirty1, 0 );
		FRepHandleIterator HandleIterator( ChangelistIterator, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1 );
		PruneChangeList_r( HandleIterator, ( uint8* )Data, MergedDirty );
	}
	else
	{
		FChangelistIterator ChangelistIterator1( Dirty1, 0 );
		FRepHandleIterator HandleIterator1( ChangelistIterator1, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1 );

		FChangelistIterator ChangelistIterator2( Dirty2, 0 );
		FRepHandleIterator HandleIterator2( ChangelistIterator2, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1 );

		MergeChangeList_r( HandleIterator1, HandleIterator2, ( uint8* )Data, MergedDirty );
	}

	MergedDirty.Add( 0 );
}

void FRepLayout::SanityCheckChangeList_DynamicArray_r( 
	const int32				CmdIndex, 
	const uint8* RESTRICT	Data, 
	TArray< uint16 > &		Changed,
	int32 &					ChangedIndex ) const
{
	const FRepLayoutCmd& Cmd = Cmds[ CmdIndex ];

	FScriptArray * Array = (FScriptArray *)Data;

	// Read the jump offset
	// We won't need to actually jump over anything because we expect the change list to be pruned once we get here
	// But we can use it to verify we read the correct amount.
	const int32 ArrayChangedCount = Changed[ChangedIndex++];

	const int32 OldChangedIndex = ChangedIndex;

	Data = (uint8*)Array->GetData();

	uint16 LocalHandle = 0;

	for ( int32 i = 0; i < Array->Num(); i++ )
	{
		LocalHandle = SanityCheckChangeList_r( CmdIndex + 1, Cmd.EndCmd - 1, Data + i * Cmd.ElementSize, Changed, ChangedIndex, LocalHandle );
	}

	check( ChangedIndex - OldChangedIndex == ArrayChangedCount );	// Make sure we read correct amount
	check( Changed[ChangedIndex] == 0 );							// Make sure we are at the end

	ChangedIndex++;
}

uint16 FRepLayout::SanityCheckChangeList_r( 
	const int32				CmdStart, 
	const int32				CmdEnd, 
	const uint8* RESTRICT	Data, 
	TArray< uint16 > &		Changed,
	int32 &					ChangedIndex,
	uint16					Handle 
	) const
{
	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[ CmdIndex ];

		check( Cmd.Type != REPCMD_Return );

		Handle++;

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			if ( Handle == Changed[ChangedIndex] )
			{
				const int32 LastChangedArrayHandle = Changed[ChangedIndex];
				ChangedIndex++;
				SanityCheckChangeList_DynamicArray_r( CmdIndex, Data + Cmd.Offset, Changed, ChangedIndex );
				check( Changed[ChangedIndex] == 0 || Changed[ChangedIndex] > LastChangedArrayHandle );
			}
			CmdIndex = Cmd.EndCmd - 1;	// Jump past children of this array (the -1 because of the ++ in the for loop)
			continue;
		}

		if ( Handle == Changed[ChangedIndex] )
		{
			const int32 LastChangedArrayHandle = Changed[ChangedIndex];
			ChangedIndex++;
			check( Changed[ChangedIndex] == 0 || Changed[ChangedIndex] > LastChangedArrayHandle );
		}
	}

	return Handle;
}

void FRepLayout::SanityCheckChangeList( const uint8* RESTRICT Data, TArray< uint16 > & Changed ) const
{
	int32 ChangedIndex = 0;
	SanityCheckChangeList_r( 0, Cmds.Num() - 1, Data, Changed, ChangedIndex, 0 );
	check( Changed[ChangedIndex] == 0 );
}

class FDiffPropertiesImpl : public FRepLayoutCmdIterator< FDiffPropertiesImpl, FCmdIteratorBaseStackState >
{
public:
	FDiffPropertiesImpl( const bool bInSync, TArray< UProperty * >*	InRepNotifies, const TArray< FRepParentCmd >& InParents, const TArray< FRepLayoutCmd >& InCmds ) : 
		FRepLayoutCmdIterator( InParents, InCmds ),
		bSync( bInSync ),
		RepNotifies( InRepNotifies ),
		bDifferent( false )
	{}

	INIT_STACK( FCmdIteratorBaseStackState ) { }

	SHOULD_PROCESS_NEXT_CMD() 
	{ 
		return true;
	}

	PROCESS_ARRAY_CMD( FCmdIteratorBaseStackState ) 
	{
		if ( StackState.DataArray->Num() != StackState.ShadowArray->Num() )
		{
			bDifferent = true;

			if ( !bSync )
			{			
				UE_LOG( LogRep, Warning, TEXT( "FDiffPropertiesImpl: Array sizes different: %s %i / %i" ), *Cmd.Property->GetFullName(), StackState.DataArray->Num(), StackState.ShadowArray->Num() );
				return;
			}

			if ( !( Parents[Cmd.ParentIndex].Flags & PARENT_IsLifetime ) )
			{
				// Currently, only lifetime properties init from their defaults
				return;
			}

			// Make the shadow state match the actual state
			FScriptArrayHelper ShadowArrayHelper( (UArrayProperty *)Cmd.Property, ShadowData );
			ShadowArrayHelper.Resize( StackState.DataArray->Num() );
		}

		StackState.BaseData			= (uint8*)StackState.DataArray->GetData();
		StackState.ShadowBaseData	= (uint8*)StackState.ShadowArray->GetData();

		// Loop over array
		ProcessDataArrayElements_r( StackState, Cmd );
	}

	PROCESS_CMD( FCmdIteratorBaseStackState ) 
	{
		const FRepParentCmd& Parent = Parents[Cmd.ParentIndex];

		// Make the shadow state match the actual state at the time of send
		if ( Parent.RepNotifyCondition == REPNOTIFY_Always || !PropertiesAreIdentical( Cmd, (const void*)( Data + Cmd.Offset ), (const void*)( ShadowData + Cmd.Offset ) ) )
		{
			bDifferent = true;

			if ( !bSync )
			{			
				UE_LOG( LogRep, Warning, TEXT( "FDiffPropertiesImpl: Property different: %s" ), *Cmd.Property->GetFullName() );
				return;
			}

			if ( !( Parent.Flags & PARENT_IsLifetime ) )
			{
				// Currently, only lifetime properties init from their defaults
				return;
			}

			StoreProperty( Cmd, (void*)( Data + Cmd.Offset ), (const void*)( ShadowData + Cmd.Offset ) );

			if ( RepNotifies && Parent.Property->HasAnyPropertyFlags( CPF_RepNotify ) )
			{
				RepNotifies->AddUnique( Parent.Property );
			}
		}
		else
		{
			UE_CLOG( LogSkippedRepNotifies > 0, LogRep, Display, TEXT( "FDiffPropertiesImpl: Skipping RepNotify because values are the same: %s" ), *Cmd.Property->GetFullName() );
		}
	}

	bool					bSync;
	TArray< UProperty * >*	RepNotifies;
	bool					bDifferent;
};

bool FRepLayout::DiffProperties( TArray<UProperty*>* RepNotifies, void* RESTRICT Destination, const void* RESTRICT Source, const bool bSync ) const
{	
	FDiffPropertiesImpl DiffPropertiesImpl( bSync, RepNotifies, Parents, Cmds );

	DiffPropertiesImpl.ProcessCmds( (uint8*)Destination, (uint8*)Source );

	return DiffPropertiesImpl.bDifferent;
}

uint32 FRepLayout::AddPropertyCmd( UProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex, uint32 ParentChecksum, int32 StaticArrayIndex )
{
	const int32 Index = Cmds.AddZeroed();

	FRepLayoutCmd & Cmd = Cmds[Index];

	Cmd.Property			= Property;
	Cmd.Type				= REPCMD_Property;		// Initially set to generic type
	Cmd.Offset				= Offset;
	Cmd.ElementSize			= Property->ElementSize;
	Cmd.RelativeHandle		= RelativeHandle;
	Cmd.ParentIndex			= ParentIndex;

	Cmd.CompatibleChecksum	= FCrc::StrCrc32( *Property->GetName().ToLower(), ParentChecksum );								// Evolve checksum on name
	Cmd.CompatibleChecksum	= FCrc::StrCrc32( *Property->GetCPPType( nullptr, 0 ).ToLower(), Cmd.CompatibleChecksum );		// Evolve by property type
	Cmd.CompatibleChecksum	= FCrc::StrCrc32( *FString::Printf( TEXT( "%i" ), StaticArrayIndex ), Cmd.CompatibleChecksum );	// Evolve by StaticArrayIndex (to make all unrolled static array elements unique)

	UProperty * UnderlyingProperty = Property;
	if ( UEnumProperty * EnumProperty = Cast< UEnumProperty >( Property ) )
	{
		UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
	}

	// Try to special case to custom types we know about
	if ( UnderlyingProperty->IsA( UStructProperty::StaticClass() ) )
	{
		UStructProperty * StructProp = Cast< UStructProperty >( UnderlyingProperty );
		UScriptStruct * Struct = StructProp->Struct;
		if ( Struct->GetFName() == NAME_Vector )
		{
			Cmd.Type = REPCMD_PropertyVector;
		}
		else if ( Struct->GetFName() == NAME_Rotator )
		{
			Cmd.Type = REPCMD_PropertyRotator;
		}
		else if ( Struct->GetFName() == NAME_Plane )
		{
			Cmd.Type = REPCMD_PropertyPlane;
		}
		else if ( Struct->GetName() == TEXT( "Vector_NetQuantize100" ) )
		{
			Cmd.Type = REPCMD_PropertyVector100;
		}
		else if ( Struct->GetName() == TEXT( "Vector_NetQuantize10" ) )
		{
			Cmd.Type = REPCMD_PropertyVector10;
		}
		else if ( Struct->GetName() == TEXT( "Vector_NetQuantizeNormal" ) )
		{
			Cmd.Type = REPCMD_PropertyVectorNormal;
		}
		else if ( Struct->GetName() == TEXT( "Vector_NetQuantize" ) )
		{
			Cmd.Type = REPCMD_PropertyVectorQ;
		}
		else if ( Struct->GetName() == TEXT( "UniqueNetIdRepl" ) )
		{
			Cmd.Type = REPCMD_PropertyNetId;
		}
		else if ( Struct->GetName() == TEXT( "RepMovement" ) )
		{
			Cmd.Type = REPCMD_RepMovement;
		}
		else
		{
			UE_LOG( LogRep, VeryVerbose, TEXT( "AddPropertyCmd: Falling back to default type for property [%s]" ), *Cmd.Property->GetFullName() );
		}
	}
	else if ( UnderlyingProperty->IsA( UBoolProperty::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyBool;
	}
	else if ( UnderlyingProperty->IsA( UFloatProperty::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyFloat;
	}
	else if ( UnderlyingProperty->IsA( UIntProperty::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyInt;
	}
	else if ( UnderlyingProperty->IsA( UByteProperty::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyByte;
	}
	else if ( UnderlyingProperty->IsA( UObjectPropertyBase::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyObject;
	}
	else if ( UnderlyingProperty->IsA( UNameProperty::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyName;
	}
	else if ( UnderlyingProperty->IsA( UUInt32Property::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyUInt32;
	}
	else if ( UnderlyingProperty->IsA( UUInt64Property::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyUInt64;
	}
	else if ( UnderlyingProperty->IsA( UStrProperty::StaticClass() ) )
	{
		Cmd.Type = REPCMD_PropertyString;
	}
	else
	{
		UE_LOG( LogRep, VeryVerbose, TEXT( "AddPropertyCmd: Falling back to default type for property [%s]" ), *Cmd.Property->GetFullName() );
	}

	return Cmd.CompatibleChecksum;
}

uint32 FRepLayout::AddArrayCmd( UArrayProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex, uint32 ParentChecksum, int32 StaticArrayIndex )
{
	const int32 Index = Cmds.AddZeroed();

	FRepLayoutCmd & Cmd = Cmds[Index];

	Cmd.Type				= REPCMD_DynamicArray;
	Cmd.Property			= Property;
	Cmd.Offset				= Offset;
	Cmd.ElementSize			= Property->Inner->ElementSize;
	Cmd.RelativeHandle		= RelativeHandle;
	Cmd.ParentIndex			= ParentIndex;
	
	Cmd.CompatibleChecksum	= FCrc::StrCrc32( *Property->GetName().ToLower(), ParentChecksum );								// Evolve checksum on name
	Cmd.CompatibleChecksum	= FCrc::StrCrc32( *Property->GetCPPType( nullptr, 0 ).ToLower(), Cmd.CompatibleChecksum );		// Evolve by property type
	Cmd.CompatibleChecksum	= FCrc::StrCrc32( *FString::Printf( TEXT( "%i" ), StaticArrayIndex ), Cmd.CompatibleChecksum );	// Evolve by StaticArrayIndex (to make all unrolled static array elements unique)

	return Cmd.CompatibleChecksum;
}

void FRepLayout::AddReturnCmd()
{
	const int32 Index = Cmds.AddZeroed();
	
	FRepLayoutCmd & Cmd = Cmds[Index];

	Cmd.Type = REPCMD_Return;
}

int32 FRepLayout::InitFromProperty_r( UProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex, uint32 ParentChecksum, int32 StaticArrayIndex )
{
	UArrayProperty * ArrayProp = Cast< UArrayProperty >( Property );

	if ( ArrayProp != NULL )
	{
		const int32 CmdStart = Cmds.Num();

		RelativeHandle++;

		const uint32 ArrayChecksum = AddArrayCmd( ArrayProp, Offset + ArrayProp->GetOffset_ForGC(), RelativeHandle, ParentIndex, ParentChecksum, StaticArrayIndex );

		InitFromProperty_r( ArrayProp->Inner, 0, 0, ParentIndex, ArrayChecksum, 0 );
		
		AddReturnCmd();

		Cmds[CmdStart].EndCmd = Cmds.Num();		// Patch in the offset to jump over our array inner elements

		return RelativeHandle;
	}

	UStructProperty * StructProp = Cast< UStructProperty >( Property );

	if ( StructProp != NULL )
	{
		UScriptStruct * Struct = StructProp->Struct;

		if ( Struct->StructFlags & STRUCT_NetDeltaSerializeNative )
		{
			// Custom delta serializers handles outside of FRepLayout
			return RelativeHandle;
		}

		if ( Struct->StructFlags & STRUCT_NetSerializeNative )
		{
			RelativeHandle++;
			AddPropertyCmd( Property, Offset + Property->GetOffset_ForGC(), RelativeHandle, ParentIndex, ParentChecksum, StaticArrayIndex );
			return RelativeHandle;
		}

		TArray< UProperty * > NetProperties;		// Track properties so me can ensure they are sorted by offsets at the end

		for ( TFieldIterator<UProperty> It( Struct ); It; ++It )
		{
			if ( ( It->PropertyFlags & CPF_RepSkip ) )
			{
				continue;
			}

			NetProperties.Add( *It );
		}

		// Sort NetProperties by memory offset
		struct FCompareUFieldOffsets
		{
			FORCEINLINE bool operator()( UProperty & A, UProperty & B ) const
			{
				// Ensure stable sort
				if ( A.GetOffset_ForGC() == B.GetOffset_ForGC() )
				{
					return A.GetName() < B.GetName();
				}

				return A.GetOffset_ForGC() < B.GetOffset_ForGC();
			}
		};

		Sort( NetProperties.GetData(), NetProperties.Num(), FCompareUFieldOffsets() );

		// Evolve checksum on struct name
		uint32 StructChecksum = FCrc::StrCrc32( *Property->GetName().ToLower(), ParentChecksum );

		// Evolve by property type
		StructChecksum = FCrc::StrCrc32( *Property->GetCPPType( nullptr, 0 ).ToLower(), StructChecksum );

		// Evolve by StaticArrayIndex (to make all unrolled static array elements unique)
		StructChecksum = FCrc::StrCrc32( *FString::Printf( TEXT( "%i" ), StaticArrayIndex ), StructChecksum );

		for ( int32 i = 0; i < NetProperties.Num(); i++ )
		{
			for ( int32 j = 0; j < NetProperties[i]->ArrayDim; j++ )
			{
				RelativeHandle = InitFromProperty_r( NetProperties[i], Offset + StructProp->GetOffset_ForGC() + j * NetProperties[i]->ElementSize, RelativeHandle, ParentIndex, StructChecksum, j );
			}
		}
		return RelativeHandle;
	}

	// Add actual property
	RelativeHandle++;

	AddPropertyCmd( Property, Offset + Property->GetOffset_ForGC(), RelativeHandle, ParentIndex, ParentChecksum, StaticArrayIndex );

	return RelativeHandle;
}

uint16 FRepLayout::AddParentProperty( UProperty * Property, int32 ArrayIndex )
{
	return Parents.Add( FRepParentCmd( Property, ArrayIndex ) );
}

void FRepLayout::InitFromObjectClass( UClass * InObjectClass )
{
	RoleIndex				= -1;
	RemoteRoleIndex			= -1;
	FirstNonCustomParent	= -1;

	int32 RelativeHandle	= 0;
	int32 LastOffset		= -1;

	Parents.Empty();

	for ( int32 i = 0; i < InObjectClass->ClassReps.Num(); i++ )
	{
		UProperty * Property	= InObjectClass->ClassReps[i].Property;
		const int32 ArrayIdx	= InObjectClass->ClassReps[i].Index;

		check( Property->PropertyFlags & CPF_Net );

		const int32 ParentHandle = AddParentProperty( Property, ArrayIdx );

		check( ParentHandle == i );
		check( Parents[i].Property->RepIndex + Parents[i].ArrayIndex == i );

		Parents[ParentHandle].CmdStart = Cmds.Num();
		RelativeHandle = InitFromProperty_r( Property, Property->ElementSize * ArrayIdx, RelativeHandle, ParentHandle, 0, ArrayIdx );
		Parents[ParentHandle].CmdEnd = Cmds.Num();
		Parents[ParentHandle].Flags |= PARENT_IsConditional;

		if ( Parents[i].CmdEnd > Parents[i].CmdStart )
		{
			check( Cmds[Parents[i].CmdStart].Offset >= LastOffset );		// >= since bool's can be combined
			LastOffset = Cmds[Parents[i].CmdStart].Offset;
		}

		// Setup flags
		if ( IsCustomDeltaProperty( Property ) )
		{
			Parents[ParentHandle].Flags |= PARENT_IsCustomDelta;
		}

		if ( Property->GetPropertyFlags() & CPF_Config )
		{
			Parents[ParentHandle].Flags |= PARENT_IsConfig;
		}

		// Hijack the first non custom property for identifying this as a rep layout block
		if ( FirstNonCustomParent == -1 && Property->ArrayDim == 1 && ( Parents[ParentHandle].Flags & PARENT_IsCustomDelta ) == 0 )
		{
			FirstNonCustomParent = ParentHandle;
		}

		// Find Role/RemoteRole property indexes so we can swap them on the client
		if ( Property->GetFName() == NAME_Role )
		{
			check( RoleIndex == -1 );
			check( Parents[ParentHandle].CmdEnd == Parents[ParentHandle].CmdStart + 1 );
			RoleIndex = ParentHandle;
		}

		if ( Property->GetFName() == NAME_RemoteRole )
		{
			check( RemoteRoleIndex == -1 );
			check( Parents[ParentHandle].CmdEnd == Parents[ParentHandle].CmdStart + 1 );
			RemoteRoleIndex = ParentHandle;
		}
	}

	// Make sure it either found both, or didn't find either
	check( ( RoleIndex == -1 ) == ( RemoteRoleIndex == -1 ) );

	// This is so the receiving side can swap these as it receives them
	if ( RoleIndex != -1 )
	{
		Parents[RoleIndex].RoleSwapIndex = RemoteRoleIndex;
		Parents[RemoteRoleIndex].RoleSwapIndex = RoleIndex;
	}
	
	AddReturnCmd();

	// Initialize lifetime props
	TArray< FLifetimeProperty >	LifetimeProps;			// Properties that replicate for the lifetime of the channel

	UObject* Object = InObjectClass->GetDefaultObject();

	Object->GetLifetimeReplicatedProps( LifetimeProps );

	// Setup lifetime replicated properties
	for ( int32 i = 0; i < LifetimeProps.Num(); i++ )
	{
		// Store the condition on the parent in case we need it
		Parents[LifetimeProps[i].RepIndex].Condition			= LifetimeProps[i].Condition;
		Parents[LifetimeProps[i].RepIndex].RepNotifyCondition	= LifetimeProps[i].RepNotifyCondition;

		if ( Parents[LifetimeProps[i].RepIndex].Flags & PARENT_IsCustomDelta )
		{
			continue;		// We don't handle custom properties in the FRepLayout class
		}

		Parents[LifetimeProps[i].RepIndex].Flags |= PARENT_IsLifetime;

		if ( LifetimeProps[i].RepIndex == RemoteRoleIndex )
		{
			// We handle remote role specially, since it can change between connections when downgraded
			// So we force it on the conditional list
			check( LifetimeProps[i].Condition == COND_None );
			LifetimeProps[i].Condition = COND_Custom;
			continue;		
		}

		if ( LifetimeProps[i].Condition == COND_None )
		{
			Parents[LifetimeProps[i].RepIndex].Flags &= ~PARENT_IsConditional;
		}			
	}

	BuildHandleToCmdIndexTable_r( 0, Cmds.Num() - 1, BaseHandleToCmdIndex );

	Owner = InObjectClass;
}

void FRepLayout::InitFromFunction( UFunction * InFunction )
{
	int32 RelativeHandle = 0;

	for ( TFieldIterator<UProperty> It( InFunction ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
	{
		for ( int32 ArrayIdx = 0; ArrayIdx < It->ArrayDim; ++ArrayIdx )
		{
			const int32 ParentHandle = AddParentProperty( *It, ArrayIdx );
			Parents[ParentHandle].CmdStart = Cmds.Num();
			RelativeHandle = InitFromProperty_r( *It, It->ElementSize * ArrayIdx, RelativeHandle, ParentHandle, 0, ArrayIdx );
			Parents[ParentHandle].CmdEnd = Cmds.Num();
		}
	}

	AddReturnCmd();

	BuildHandleToCmdIndexTable_r( 0, Cmds.Num() - 1, BaseHandleToCmdIndex );

	Owner = InFunction;
}

void FRepLayout::InitFromStruct( UStruct * InStruct )
{
	int32 RelativeHandle = 0;

	for ( TFieldIterator<UProperty> It( InStruct ); It; ++It )
	{
		if ( It->PropertyFlags & CPF_RepSkip )
		{
			continue;
		}
			
		for ( int32 ArrayIdx = 0; ArrayIdx < It->ArrayDim; ++ArrayIdx )
		{
			const int32 ParentHandle = AddParentProperty( *It, ArrayIdx );
			Parents[ParentHandle].CmdStart = Cmds.Num();
			RelativeHandle = InitFromProperty_r( *It, It->ElementSize * ArrayIdx, RelativeHandle, ParentHandle, 0, ArrayIdx );
			Parents[ParentHandle].CmdEnd = Cmds.Num();
		}
	}

	AddReturnCmd();

	BuildHandleToCmdIndexTable_r( 0, Cmds.Num() - 1, BaseHandleToCmdIndex );

	Owner = InStruct;
}

void FRepLayout::SerializeProperties_DynamicArray_r( 
	FArchive &			Ar, 
	UPackageMap	*		Map,
	const int32			CmdIndex,
	uint8 *				Data,
	bool &				bHasUnmapped ) const
{
	const FRepLayoutCmd& Cmd = Cmds[ CmdIndex ];

	FScriptArray * Array = (FScriptArray *)Data;	

	uint16 OutArrayNum = Array->Num();
	Ar << OutArrayNum;

	// If loading from the archive, OutArrayNum will contain the number of elements.
	// Otherwise, use the input number of elements.
	const int32 ArrayNum = Ar.IsLoading() ? (int32)OutArrayNum : Array->Num();

	// Validate the maximum number of elements.
	if (ArrayNum > MaxRepArraySize)
	{
		UE_LOG(LogRepTraffic, Error, TEXT("SerializeProperties_DynamicArray_r: ArraySize (%d) > net.MaxRepArraySize(%d) (%s). net.MaxRepArraySize can be updated in Project Settings under Network Settings."),
			ArrayNum, MaxRepArraySize, *Cmd.Property->GetName());

		Ar.SetError();
	}
	// Validate the maximum memory.
	else if (ArrayNum * (int32)Cmd.ElementSize > MaxRepArrayMemory)
	{
		UE_LOG(LogRepTraffic, Error,
			TEXT("SerializeProperties_DynamicArray_r: ArraySize (%d) * Cmd.ElementSize (%d) > net.MaxRepArrayMemory(%d) (%s). net.MaxRepArrayMemory can be updated in Project Settings under Network Settings."),
			ArrayNum, (int32)Cmd.ElementSize, MaxRepArrayMemory, *Cmd.Property->GetName());

		Ar.SetError();
	}

	if (!Ar.IsError())
	{
		// When loading, we may need to resize the array to properly fit the number of elements.
		if (Ar.IsLoading() && OutArrayNum != Array->Num())
		{
			FScriptArrayHelper ArrayHelper((UArrayProperty *)Cmd.Property, Data);
			ArrayHelper.Resize(OutArrayNum);
		}

		Data = (uint8*)Array->GetData();

		for (int32 i = 0; i < Array->Num() && !Ar.IsError(); i++)
		{
			SerializeProperties_r(Ar, Map, CmdIndex + 1, Cmd.EndCmd - 1, Data + i * Cmd.ElementSize, bHasUnmapped);
		}
	}	
}

void FRepLayout::SerializeProperties_r( 
	FArchive &			Ar, 
	UPackageMap	*		Map,
	const int32			CmdStart, 
	const int32			CmdEnd,
	void *				Data,
	bool &				bHasUnmapped ) const
{
	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd && !Ar.IsError(); CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		check( Cmd.Type != REPCMD_Return );

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			SerializeProperties_DynamicArray_r( Ar, Map, CmdIndex, (uint8*)Data + Cmd.Offset, bHasUnmapped );
			CmdIndex = Cmd.EndCmd - 1;		// The -1 to handle the ++ in the for loop
			continue;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVarDoReplicationContextString->GetInt() > 0)
		{
			Map->SetDebugContextString( FString::Printf(TEXT("%s - %s"), *Owner->GetPathName(), *Cmd.Property->GetPathName() ) );
		}
#endif

		if ( !Cmd.Property->NetSerializeItem( Ar, Map, (void*)( (uint8*)Data + Cmd.Offset ) ) )
		{
			bHasUnmapped = true;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVarDoReplicationContextString->GetInt() > 0)
		{
			Map->ClearDebugContextString();
		}
#endif
	}
}

void FRepLayout::BuildChangeList_r( const TArray< FHandleToCmdIndex >& HandleToCmdIndex, const int32 CmdStart, const int32 CmdEnd, uint8* Data, const int32 HandleOffset, TArray< uint16 >& Changed ) const
{
	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		check( Cmd.Type != REPCMD_Return );

		if ( Cmd.Type == REPCMD_DynamicArray )
		{			
			FScriptArray* Array = ( FScriptArray * )( Data + Cmd.Offset );

			TArray< uint16 > ChangedLocal;

			TArray< FHandleToCmdIndex >& ArrayHandleToCmdIndex = *HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex;

			const int32 ArrayCmdStart			= CmdIndex + 1;
			const int32 ArrayCmdEnd				= Cmd.EndCmd - 1;
			const int32 NumHandlesPerElement	= ArrayHandleToCmdIndex.Num();

			check( NumHandlesPerElement > 0 );

			for ( int32 i = 0; i < Array->Num(); i++ )
			{
				BuildChangeList_r( ArrayHandleToCmdIndex, ArrayCmdStart, ArrayCmdEnd, ((uint8*)Array->GetData()) + Cmd.ElementSize * i, i * NumHandlesPerElement, ChangedLocal );
			}

			if ( ChangedLocal.Num() )
			{
				Changed.Add( Cmd.RelativeHandle + HandleOffset );	// Identify the array cmd handle
				Changed.Add( ChangedLocal.Num() );					// This is so we can jump over the array if we need to
				Changed.Append( ChangedLocal );						// Append the change list under the array
				Changed.Add( 0 );									// Null terminator
			}

			CmdIndex = Cmd.EndCmd - 1;		// The -1 to handle the ++ in the for loop
			continue;
		}

		Changed.Add( Cmd.RelativeHandle + HandleOffset );
	}
}

void FRepLayout::SendPropertiesForRPC( UObject* Object, UFunction * Function, UActorChannel * Channel, FNetBitWriter & Writer, void* Data ) const
{
	check( Function == Owner );

	if ( Channel->Connection->InternalAck )
	{
		TArray< uint16 > Changed;

		for ( int32 i = 0; i < Parents.Num(); i++ )
		{
			if ( !Parents[i].Property->Identical_InContainer( Data, NULL, Parents[i].ArrayIndex ) )
			{
				BuildChangeList_r( BaseHandleToCmdIndex, Parents[i].CmdStart, Parents[i].CmdEnd, ( uint8* )Data, 0, Changed );
			}
		}

		Changed.Add( 0 ); // Null terminator

		SendProperties_BackwardsCompatible( nullptr, nullptr, ( uint8* )Data, Channel->Connection, Writer, Changed );

		return;
	}

	for ( int32 i = 0; i < Parents.Num(); i++ )
	{
		bool Send = true;

		if ( !Cast<UBoolProperty>( Parents[i].Property ) )
		{
			// check for a complete match, including arrays
			// (we're comparing against zero data here, since 
			// that's the default.)
			Send = !Parents[i].Property->Identical_InContainer( Data, NULL, Parents[i].ArrayIndex );

			Writer.WriteBit( Send ? 1 : 0 );
		}

		if ( Send )
		{
			bool bHasUnmapped = false;
			SerializeProperties_r( Writer, Writer.PackageMap, Parents[i].CmdStart, Parents[i].CmdEnd, Data, bHasUnmapped );
		}
	}
}

void FRepLayout::ReceivePropertiesForRPC( UObject* Object, UFunction * Function, UActorChannel * Channel, FNetBitReader & Reader, void* Data, TSet<FNetworkGUID>& UnmappedGuids) const
{
	check( Function == Owner );

	for ( int32 i = 0; i < Parents.Num(); i++ )
	{
		if ( Parents[i].ArrayIndex == 0 && ( Parents[i].Property->PropertyFlags & CPF_ZeroConstructor ) == 0 )
		{
			// If this property needs to be constructed, make sure we do that
			Parents[i].Property->InitializeValue((uint8*)Data + Parents[i].Property->GetOffset_ForUFunction());
		}
	}

	if ( Channel->Connection->InternalAck )
	{
		bool bHasUnmapped = false;
		bool bGuidsChanged = false;

		// Let package map know we want to track and know about any guids that are unmapped during the serialize call
		// We have to do this manually since we aren't passing in any unmapped info
		Reader.PackageMap->ResetTrackedGuids( true );

		ReceiveProperties_BackwardsCompatible( Channel->Connection, nullptr, Data, Reader, bHasUnmapped, false, bGuidsChanged );

		if ( Reader.PackageMap->GetTrackedUnmappedGuids().Num() > 0 )
		{
			bHasUnmapped = true;
			UnmappedGuids = Reader.PackageMap->GetTrackedUnmappedGuids();
		}

		Reader.PackageMap->ResetTrackedGuids( false );

		if ( bHasUnmapped )
		{
			UE_LOG( LogRepTraffic, Log, TEXT( "Unable to resolve RPC parameter to do being unmapped. Object[%d] %s. Function %s." ),
					Channel->ChIndex, *Object->GetName(), *Function->GetName() );
		}
	}
	else
	{
		Reader.PackageMap->ResetTrackedGuids(true);

		for ( int32 i = 0; i < Parents.Num(); i++ )
		{
			if ( Cast<UBoolProperty>( Parents[i].Property ) || Reader.ReadBit() )
			{
				bool bHasUnmapped = false;

				SerializeProperties_r( Reader, Reader.PackageMap, Parents[i].CmdStart, Parents[i].CmdEnd, Data, bHasUnmapped );

				if ( Reader.IsError() )
				{
					return;
				}

				if ( bHasUnmapped )
				{
					UE_LOG( LogRepTraffic, Log, TEXT( "Unable to resolve RPC parameter. Object[%d] %s. Function %s. Parameter %s." ),
							Channel->ChIndex, *Object->GetName(), *Function->GetName(), *Parents[i].Property->GetName() );
				}
			}
		}

		if (Reader.PackageMap->GetTrackedUnmappedGuids().Num() > 0)
		{
			UnmappedGuids = Reader.PackageMap->GetTrackedUnmappedGuids();
		}

		Reader.PackageMap->ResetTrackedGuids(false);
	}
}

void FRepLayout::SerializePropertiesForStruct( UStruct * Struct, FArchive & Ar, UPackageMap	* Map, void* Data, bool & bHasUnmapped ) const
{
	check( Struct == Owner );

	for ( int32 i = 0; i < Parents.Num(); i++ )
	{
		SerializeProperties_r( Ar, Map, Parents[i].CmdStart, Parents[i].CmdEnd, Data, bHasUnmapped );

		if ( Ar.IsError() )
		{
			return;
		}
	}
}

void FRepLayout::BuildHandleToCmdIndexTable_r( const int32 CmdStart, const int32 CmdEnd, TArray< FHandleToCmdIndex >& HandleToCmdIndex )
{
	for ( int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++ )
	{
		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

		check( Cmd.Type != REPCMD_Return );

		const int32 Index = HandleToCmdIndex.Add( FHandleToCmdIndex( CmdIndex ) );

		if ( Cmd.Type == REPCMD_DynamicArray )
		{
			HandleToCmdIndex[Index].HandleToCmdIndex = TUniquePtr< TArray< FHandleToCmdIndex > >( new TArray< FHandleToCmdIndex >() );

			TArray< FHandleToCmdIndex >& ArrayHandleToCmdIndex = *HandleToCmdIndex[Index].HandleToCmdIndex;

			BuildHandleToCmdIndexTable_r( CmdIndex + 1, Cmd.EndCmd - 1, ArrayHandleToCmdIndex );
			CmdIndex = Cmd.EndCmd - 1;		// The -1 to handle the ++ in the for loop
		}
	}
}

void FRepLayout::RebuildConditionalProperties( FRepState * RESTRICT	RepState, const FRepChangedPropertyTracker& ChangedTracker, const FReplicationFlags& RepFlags ) const
{
	SCOPE_CYCLE_COUNTER( STAT_NetRebuildConditionalTime );
	
	// Setup condition map
	const bool bIsInitial	= RepFlags.bNetInitial		? true : false;
	const bool bIsOwner		= RepFlags.bNetOwner		? true : false;
	const bool bIsSimulated	= RepFlags.bNetSimulated	? true : false;
	const bool bIsPhysics	= RepFlags.bRepPhysics		? true : false;
	const bool bIsReplay	= RepFlags.bReplay			? true : false;

	RepState->ConditionMap[COND_None]						= true;
	RepState->ConditionMap[COND_InitialOnly]				= bIsInitial;

	RepState->ConditionMap[COND_OwnerOnly] = bIsOwner;
	RepState->ConditionMap[COND_SkipOwner] = !bIsOwner;

	RepState->ConditionMap[COND_SimulatedOnly] = bIsSimulated;
	RepState->ConditionMap[COND_SimulatedOnlyNoReplay] = bIsSimulated && !bIsReplay;
	RepState->ConditionMap[COND_AutonomousOnly] = !bIsSimulated;

	RepState->ConditionMap[COND_SimulatedOrPhysics] = bIsSimulated || bIsPhysics;
	RepState->ConditionMap[COND_SimulatedOrPhysicsNoReplay] = (bIsSimulated || bIsPhysics) && !bIsReplay;

	RepState->ConditionMap[COND_InitialOrOwner] = bIsInitial || bIsOwner;
	RepState->ConditionMap[COND_ReplayOrOwner] = bIsReplay || bIsOwner;
	RepState->ConditionMap[COND_ReplayOnly] = bIsReplay;
	RepState->ConditionMap[COND_SkipReplay] = !bIsReplay;

	RepState->ConditionMap[COND_Custom] = true;

	RepState->RepFlags = RepFlags;
}

void FRepLayout::InitChangedTracker(FRepChangedPropertyTracker * ChangedTracker) const
{
	ChangedTracker->Parents.SetNum(Parents.Num());

	for (int32 i = 0; i < Parents.Num(); i++)
	{
		ChangedTracker->Parents[i].IsConditional = (Parents[i].Flags & PARENT_IsConditional) ? 1 : 0;
	}
}

void FRepLayout::InitShadowData(
	TArray< uint8, TAlignedHeapAllocator<16> >&	ShadowData,
	UClass *									InObjectClass,
	uint8 *										Src ) const
{
	ShadowData.Empty();
	ShadowData.AddZeroed( InObjectClass->GetDefaultsCount() );

	// Construct the properties
	ConstructProperties( ShadowData );

	// Init the properties
	InitProperties( ShadowData, Src );
}

void FRepLayout::InitRepState(
	FRepState*									RepState,
	UClass *									InObjectClass, 
	uint8 *										Src, 
	TSharedPtr< FRepChangedPropertyTracker > &	InRepChangedPropertyTracker ) const
{
	InitShadowData( RepState->StaticBuffer, InObjectClass, Src );

	RepState->RepChangedPropertyTracker = InRepChangedPropertyTracker;

	check( RepState->RepChangedPropertyTracker->Parents.Num() == Parents.Num() );

	// Start out the conditional props based on a default RepFlags struct
	// It will rebuild if it ever changes
	RebuildConditionalProperties( RepState, *InRepChangedPropertyTracker.Get(), FReplicationFlags() );
}

void FRepLayout::ConstructProperties( TArray< uint8, TAlignedHeapAllocator<16> >& ShadowData ) const
{
	uint8* StoredData = ShadowData.GetData();

	// Construct all items
	for ( int32 i = 0; i < Parents.Num(); i++ )
	{
		// Only construct the 0th element of static arrays (InitializeValue will handle the elements)
		if ( Parents[i].ArrayIndex == 0 )
		{
			PTRINT Offset = Parents[i].Property->ContainerPtrToValuePtr<uint8>( StoredData ) - StoredData;
			check( Offset >= 0 && Offset < ShadowData.Num() );

			Parents[i].Property->InitializeValue( StoredData + Offset );
		}
	}
}

void FRepLayout::InitProperties( TArray< uint8, TAlignedHeapAllocator<16> >& ShadowData, uint8* Src ) const
{
	LLM_SCOPE(ELLMTag::Networking);

	uint8* StoredData = ShadowData.GetData();

	// Init all items
	for ( int32 i = 0; i < Parents.Num(); i++ )
	{
		// Only copy the 0th element of static arrays (CopyCompleteValue will handle the elements)
		if ( Parents[i].ArrayIndex == 0 )
		{
			PTRINT Offset = Parents[i].Property->ContainerPtrToValuePtr<uint8>( StoredData ) - StoredData;
			check( Offset >= 0 && Offset < ShadowData.Num() );

			Parents[i].Property->CopyCompleteValue( StoredData + Offset, Src + Offset );
		}
	}
}

void FRepLayout::DestructProperties( FRepStateStaticBuffer& RepStateStaticBuffer ) const
{
	uint8* StoredData = RepStateStaticBuffer.GetData();

	// Destruct all items
	for ( int32 i = 0; i < Parents.Num(); i++ )
	{
		// Only copy the 0th element of static arrays (DestroyValue will handle the elements)
		if ( Parents[i].ArrayIndex == 0 )
		{
			PTRINT Offset = Parents[i].Property->ContainerPtrToValuePtr<uint8>( StoredData ) - StoredData;
			check( Offset >= 0 && Offset < RepStateStaticBuffer.Num() );

			Parents[i].Property->DestroyValue( StoredData + Offset );
		}
	}

	RepStateStaticBuffer.Empty();
}

void FRepLayout::GetLifetimeCustomDeltaProperties(TArray< int32 > & OutCustom, TArray< ELifetimeCondition >	& OutConditions)
{
	OutCustom.Empty();
	OutConditions.Empty();

	for ( int32 i = 0; i < Parents.Num(); i++ )
	{
		if ( Parents[i].Flags & PARENT_IsCustomDelta )
		{
			check( Parents[i].Property->RepIndex + Parents[i].ArrayIndex == i );

			OutCustom.Add(i);
			OutConditions.Add(Parents[i].Condition);
		}
	}
}

void FRepLayout::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (int32 i = 0; i < Parents.Num(); i++)
	{
		if (Parents[i].Property != nullptr)
		{
			Collector.AddReferencedObject(Parents[i].Property);
		}
	}
}


FRepState::~FRepState()
{
	if (RepLayout.IsValid() && StaticBuffer.Num() > 0)
	{	
		RepLayout->DestructProperties( StaticBuffer );
	}
}

FRepChangelistState::~FRepChangelistState()
{
	if (RepLayout.IsValid() && StaticBuffer.Num() > 0)
	{	
		RepLayout->DestructProperties( StaticBuffer );
	}
}
