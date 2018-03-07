// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkProfiler.h: network profiling support.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class FOutBunch;
class UNetConnection;
struct FURL;

#if USE_NETWORK_PROFILER 

#define NETWORK_PROFILER( x ) if ( GNetworkProfiler.IsTrackingEnabled() ) { x; }

/*=============================================================================
	Network profiler header.
=============================================================================*/

class FNetworkProfilerHeader
{
private:
	/** Magic to ensure we're opening the right file.	*/
	uint32	Magic;
	/** Version number to detect version mismatches.	*/
	uint32	Version;

	/** Tag, set via -networkprofiler=TAG				*/
	FString Tag;
	/** Game name, e.g. Example							*/
	FString GameName;
	/** URL used to open/ browse to the map.			*/
	FString URL;

public:
	/** Constructor.									*/
	FNetworkProfilerHeader();

	/** Resets the header info for a new session.		*/
	void Reset(const FURL& InURL);

	/** Returns the URL stored in the header.			*/
	FString GetURL() const { return URL; }

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FNetworkProfilerHeader& Header );
};

/*=============================================================================
	FNetworkProfiler
=============================================================================*/

/**
 * Network profiler, using serialized token emission like e.g. script and malloc profiler.
 */
class FNetworkProfiler
{
private:
	/** File writer used to serialize data.															*/
	FArchive*								FileWriter;

	/** Critical section to sequence tracking.														*/
	FCriticalSection						CriticalSection;

	/** Mapping from name to index in name array.													*/
	TMap<FString,int32>						NameToNameTableIndexMap;

	/** Array of unique names.																		*/
	TArray<FString>							NameArray;

	/** Mapping from address to index in address array.												*/
	TMap<uint64, int32>						AddressTableIndexMap;

	/** Array of unique addresses																	*/
	TArray<uint64>							AddressArray;

	/** Whether noticeable network traffic has occured in this session. Used to discard it.			*/
	bool									bHasNoticeableNetworkTrafficOccured;
	/** Whether tracking is enabled.																*/
	bool									bIsTrackingEnabled;	

	/** Header for the current session.																*/
	FNetworkProfilerHeader					CurrentHeader;

	/** Last known address																			*/
	uint64									LastAddress;

	/** All the data required for writing sent bunches to the profiler stream						*/
	struct FSendBunchInfo
	{
		uint16 ChannelIndex;
		uint8 ChannelType;
		uint16 NumHeaderBits;
		uint16 NumPayloadBits;

		FSendBunchInfo()
			: ChannelIndex(0)
			, ChannelType(0)
			, NumHeaderBits(0)
			, NumPayloadBits(0) {}

		FSendBunchInfo( uint16 InChannelIndex, uint8 InChannelType, uint16 InNumHeaderBits, uint16 InNumPayloadBits )
			: ChannelIndex(InChannelIndex)
			, ChannelType(InChannelType)
			, NumHeaderBits(InNumHeaderBits)
			, NumPayloadBits(InNumPayloadBits) {}
	};

	/** Stack outgoing bunches per connection, the top bunch for a connection may be popped if it gets merged with a new bunch.		*/
	TMap<UNetConnection*, TArray<FSendBunchInfo>>	OutgoingBunches;

	/** Data required to write queued RPCs to the profiler stream */
	struct FQueuedRPCInfo
	{
		UNetConnection* Connection;
		UObject* TargetObject;
		uint32 ActorNameIndex;
		uint32 FunctionNameIndex;
		uint16 NumHeaderBits;
		uint16 NumParameterBits;
		uint16 NumFooterBits;

		FQueuedRPCInfo()
			: Connection(nullptr)
			, TargetObject(nullptr)
			, ActorNameIndex(0)
			, FunctionNameIndex(0)
			, NumHeaderBits(0)
			, NumParameterBits(0)
			, NumFooterBits(0) {}
	};
	
	TArray<FQueuedRPCInfo> QueuedRPCs;

	/**
	 * Returns index of passed in name into name array. If not found, adds it.
	 *
	 * @param	Name	Name to find index for
	 * @return	Index of passed in name
	 */
	int32 GetNameTableIndex( const FString& Name );

	/**
	* Returns index of passed in address into address array. If not found, adds it.
	*
	* @param	Address	Address to find index for
	* @return	Index of passed in name
	*/
	int32 GetAddressTableIndex( uint64 Address );

public:
	/**
	 * Constructor, initializing members.
	 */
	FNetworkProfiler();

	/**
	 * Enables/ disables tracking. Emits a session changes if disabled.
	 *
	 * @param	bShouldEnableTracking	Whether tracking should be enabled or not
	 */
	void EnableTracking( bool bShouldEnableTracking );

	/**
	 * Marks the beginning of a frame.
	 */
	ENGINE_API void TrackFrameBegin();

	/**
	* Tracks when connection address changes
	*/
	ENGINE_API void SetCurrentConnection( UNetConnection* Connection );

	
	/**
	 * Tracks and RPC being sent.
	 * 
	 * @param	Actor				Actor RPC is being called on
	 * @param	Function			Function being called
	 * @param	NumHeaderBits		Number of bits serialized into the header for this RPC
	 * @param	NumParameterBits	Number of bits serialized into parameters of this RPC
	 * @param	NumFooterBits		Number of bits serialized into the footer of this RPC (EndContentBlock)
	 */
	void TrackSendRPC( const AActor* Actor, const UFunction* Function, uint16 NumHeaderBits, uint16 NumParameterBits, uint16 NumFooterBits, UNetConnection* Connection );
	
	/**
	 * Tracks queued RPCs (unreliable multicast) being sent.
	 * 
	 * @param	Connection			The connection on which this RPC is queued
	 * @param	TargetObject		The target object of the RPC
	 * @param	Actor				Actor RPC is being called on
	 * @param	Function			Function being called
	 * @param	NumHeaderBits		Number of bits serialized into the header for this RPC
	 * @param	NumParameterBits	Number of bits serialized into parameters of this RPC
	 * @param	NumFooterBits		Number of bits serialized into the footer of this RPC (EndContentBlock)
	 */
	void TrackQueuedRPC( UNetConnection* Connection, UObject* TargetObject, const AActor* Actor, const UFunction* Function, uint16 NumHeaderBits, uint16 NumParameterBits, uint16 NumFooterBits );

	/**
	 * Writes all queued RPCs for the connection to the profiler stream
	 *
	 * @param Connection The connection for which RPCs are being flushed
	 * @param TargetObject The target object of the RPC
	 */
	void FlushQueuedRPCs( UNetConnection* Connection, UObject* TargetObject );

	/**
	 * Low level FSocket::Send information.
	 *
	 * @param	SocketDesc				Description of socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 */
	ENGINE_API void TrackSocketSend( const FString& SocketDesc, const void* Data, uint16 BytesSent );

	/**
	 * Low level FSocket::SendTo information.
	 *
 	 * @param	SocketDesc				Description of socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 * @param	NumPacketIdBits			Number of bits sent for the packet id
	 * @param	NumBunchBits			Number of bits sent in bunches
	 * @param	NumAckBits				Number of bits sent in acks
	 * @param	NumPaddingBits			Number of bits appended to the end to byte-align the data
	 * @param	Connection				Destination address
	 */
	ENGINE_API void TrackSocketSendTo(
		const FString& SocketDesc,
		const void* Data,
		uint16 BytesSent,
		uint16 NumPacketIdBits,
		uint16 NumBunchBits,
		uint16 NumAckBits,
		uint16 NumPaddingBits,
		UNetConnection* Connection );

	/**
	 * Low level FSocket::SendTo information.
	 *
 	 * @param	SocketDesc				Description of socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 * @param	Connection				Destination address
	 */
	void TrackSocketSendToCore(
		const FString& SocketDesc,
		const void* Data,
		uint16 BytesSent,
		uint16 NumPacketIdBits,
		uint16 NumBunchBits,
		uint16 NumAckBits,
		uint16 NumPaddingBits,
		UNetConnection* Connection );

	
	/**
	 * Mid level UChannel::SendBunch information.
	 * 
	 * @param	OutBunch	FOutBunch being sent
	 * @param	NumBits		Num bits to serialize for this bunch (not including merging)
	 */
	void TrackSendBunch( FOutBunch* OutBunch, uint16 NumBits, UNetConnection* Connection );
	
	/**
	 * Add a sent bunch to the stack. These bunches are not written to the stream immediately,
	 * because they may be merged with another bunch in the future.
	 *
	 * @param Connection The connection on which this bunch was sent
	 * @param OutBunch The bunch being sent
	 * @param NumHeaderBits Number of bits in the bunch header
	 * @param NumPayloadBits Number of bits in the bunch, excluding the header
	 */
	void PushSendBunch( UNetConnection* Connection, FOutBunch* OutBunch, uint16 NumHeaderBits, uint16 NumPayloadBits );

	/**
	 * Pops the latest bunch for a connection, since it is going to be merged with the next bunch.
	 *
	 * @param Connection the connection which is merging a bunch
	 */
	void PopSendBunch( UNetConnection* Connection );

	/**
	 * Writes all the outgoing bunches for a connection in the stack to the profiler data stream.
	 *
	 * @param Connection the connection which is about to send any pending bunches over the network
	 */
	ENGINE_API void FlushOutgoingBunches( UNetConnection* Connection );

	/**
	 * Track actor being replicated.
	 *
	 * @param	Actor		Actor being replicated
	 */
	void TrackReplicateActor( const AActor* Actor, FReplicationFlags RepFlags, uint32 Cycles, UNetConnection* Connection );
	
	/**
	 * Track property being replicated.
	 *
	 * @param	Property	Property being replicated
	 * @param	NumBits		Number of bits used to replicate this property
	 */
	void TrackReplicateProperty( const UProperty* Property, uint16 NumBits, UNetConnection* Connection );

	/**
	 * Track property header being written.
	 *
	 * @param	Property	Property being replicated
	 * @param	NumBits		Number of bits used in the header
	 */
	void TrackWritePropertyHeader( const UProperty* Property, uint16 NumBits, UNetConnection* Connection );

	/**
	 * Track event occuring, like e.g. client join/ leave
	 *
	 * @param	EventName			Name of the event
	 * @param	EventDescription	Additional description/ information for event
	 */
	void TrackEvent( const FString& EventName, const FString& EventDescription, UNetConnection* Connection );

	/**
	 * Called when the server first starts listening and on round changes or other
	 * similar game events. We write to a dummy file that is renamed when the current
	 * session ends.
	 *
	 * @param	bShouldContinueTracking		Whether to continue tracking
	 * @param	InURL						URL used for new session
	 */
	void TrackSessionChange( bool bShouldContinueTracking, const FURL& InURL );

	/**
	 * Track sent acks.
	 *
	 * @param NumBits Number of bits in the ack
	 */
	void TrackSendAck( uint16 NumBits, UNetConnection* Connection );

	/**
	 * Track NetGUID export bunches.
	 *
	 * @param NumBits Number of bits in the GUIDs
	 */
	void TrackExportBunch( uint16 NumBits, UNetConnection* Connection );

	/**
	 * Track "must be mapped" GUIDs
	 *
	 * @param NumGuids Number of GUIDs added to the bunch
	 * @param NumBits Number of bits added to the bunch for the GUIDs
	 */
	void TrackMustBeMappedGuids( uint16 NumGuids, uint16 NumBits, UNetConnection* Connection );

	/**
	 * Track actor content block headers
	 *
	 * @param Object the object being replicated (might be a subobject of the actor)
	 * @param NumBits the number of bits in the content block header
	 */
	void TrackBeginContentBlock( UObject* Object, uint16 NumBits, UNetConnection* Connection );

	/**
	 * Track actor content block headers
	 *
	 * @param Object the object being replicated (might be a subobject of the actor)
	 * @param NumBits the number of bits in the content block footer
	 */
	void TrackEndContentBlock( UObject* Object, uint16 NumBits, UNetConnection* Connection );

	/** Track property handles
	 *
	 * @param NumBits Number of bits in the property handle
	 */
	void TrackWritePropertyHandle( uint16 NumBits, UNetConnection* Connection );

	/**
	 * Processes any network profiler specific exec commands
	 *
	 * @param InWorld	The world in this context
	 * @param Cmd		The command to parse
	 * @param Ar		The output device to write data to
	 *
	 * @return			True if processed, false otherwise
	 */
	bool Exec( UWorld * InWorld, const TCHAR* Cmd, FOutputDevice & Ar );

	bool FORCEINLINE IsTrackingEnabled() const { return bIsTrackingEnabled; }
};

/** Global network profiler instance. */
extern ENGINE_API FNetworkProfiler GNetworkProfiler;

#else	// USE_NETWORK_PROFILER

#define NETWORK_PROFILER(x)

#endif
