// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
// A network connection.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Serialization/BitWriter.h"
#include "Misc/NetworkGuid.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Engine/NetDriver.h"
#include "Net/DataBunch.h"
#include "Engine/Player.h"
#include "Engine/Channel.h"
#include "ProfilingDebugging/Histogram.h"
#include "ArrayView.h"

#include "NetConnection.generated.h"

class FObjectReplicator;
class StatelessConnectHandlerComponent;
class UActorChannel;
class UChildConnection;

/*-----------------------------------------------------------------------------
	Types.
-----------------------------------------------------------------------------*/
enum { RELIABLE_BUFFER = 256 }; // Power of 2 >= 1.
enum { MAX_PACKETID = 16384 };  // Power of 2 >= 1, covering guaranteed loss/misorder time.
enum { MAX_CHSEQUENCE = 1024 }; // Power of 2 >RELIABLE_BUFFER, covering loss/misorder time.
enum { MAX_BUNCH_HEADER_BITS = 64 };
enum { MAX_PACKET_HEADER_BITS = 15 }; // = FMath::CeilLogTwo(MAX_PACKETID) + 1 (IsAck)
enum { MAX_PACKET_TRAILER_BITS = 1 };

class UNetDriver;

//
// Whether to support net lag and packet loss testing.
//
#define DO_ENABLE_NET_TEST !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

// 
// State of a connection.
//
enum EConnectionState
{
	USOCK_Invalid   = 0, // Connection is invalid, possibly uninitialized.
	USOCK_Closed    = 1, // Connection permanently closed.
	USOCK_Pending	= 2, // Connection is awaiting connection.
	USOCK_Open      = 3, // Connection is open.
};

// 
// Security event types used for UE_SECURITY_LOG
//
namespace ESecurityEvent
{ 
	enum Type
	{
		Malformed_Packet = 0, // The packet didn't follow protocol
		Invalid_Data = 1,     // The packet contained invalid data
		Closed = 2            // The connection had issues (potentially malicious) and was closed
	};
	
	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(const ESecurityEvent::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Malformed_Packet:
			{
				return TEXT("Malformed_Packet");
			}
			case Invalid_Data:
			{
				return TEXT("Invalid_Data");
			}
			case Closed:
			{
				return TEXT("Closed");
			}
		}
		return TEXT("");
	}
}

/** If this connection is from a client, this is the current login state of this connection/login attempt */
namespace EClientLoginState
{
	enum Type
	{
		Invalid		= 0,		// This must be a client (which doesn't use this state) or uninitialized.
		LoggingIn	= 1,		// The client is currently logging in.
		Welcomed	= 2,		// Fully logged in.
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString( const EClientLoginState::Type EnumVal )
	{
		switch (EnumVal)
		{
			case Invalid:
			{
				return TEXT("Invalid");
			}
			case LoggingIn:
			{
				return TEXT("LoggingIn");
			}
			case Welcomed:
			{
				return TEXT("Welcomed");
			}
		}
		return TEXT("");
	}
};


// Delegates
#if !UE_BUILD_SHIPPING
/**
 * Delegate for hooking the net connections 'ReceivedRawPacket'
 * 
 * @param Data				The data received
 * @param Count				The number of bytes received
 * @param bBlockReceive		Whether or not to block further processing of the packet (defaults to false)
*/
DECLARE_DELEGATE_ThreeParams(FOnReceivedRawPacket, void* /*Data*/, int32 /*Count*/, bool& /*bBlockReceive*/);

/**
 * Delegate for hooking the net connections 'LowLevelSend' (at the socket level, after PacketHandler parsing)
 *
 * @param Data			The data being sent
 * @param Count			The number of bytes being sent
 * @param bBlockSend	Whether or not to block the send (defaults to false)
*/
DECLARE_DELEGATE_ThreeParams(FOnLowLevelSend, void* /*Data*/, int32 /*Count*/, bool& /*bBlockSend*/);
#endif


#if DO_ENABLE_NET_TEST
/**
 * An artificially lagged packet
 */
struct DelayedPacket
{
	/** The packet data to send */
	TArray<uint8> Data;

	/** The size of the packet in bits */
	int32 SizeBits;

	/** The time at which to send the packet */
	double SendTime;

public:
	FORCEINLINE DelayedPacket(uint8* InData, int32 InSizeBytes, int32 InSizeBits)
		: Data()
		, SizeBits(InSizeBits)
		, SendTime(0.0)
	{
		Data.AddUninitialized(InSizeBytes);
		FMemory::Memcpy(Data.GetData(), InData, InSizeBytes);
	}
};
#endif


UCLASS(customConstructor, Abstract, MinimalAPI, transient, config=Engine)
class UNetConnection : public UPlayer
{
	GENERATED_UCLASS_BODY()

	/** child connections for secondary viewports */
	UPROPERTY(transient)
	TArray<class UChildConnection*> Children;

	/** Owning net driver */
	UPROPERTY()
	class UNetDriver* Driver;	

	/** The class name for the PackageMap to be loaded */
	UPROPERTY()
	TSubclassOf<UPackageMap> PackageMapClass;

	UPROPERTY()
	/** Package map between local and remote. (negotiates net serialization) */
	class UPackageMap* PackageMap;

	/** @todo document */
	UPROPERTY()
	TArray<class UChannel*> OpenChannels;
	 
	/** This actor is bNetTemporary, which means it should never be replicated after it's initial packet is complete */
	UPROPERTY()
	TArray<class AActor*> SentTemporaries;

	/** The actor that is currently being viewed/controlled by the owning controller */
	UPROPERTY()
	class AActor* ViewTarget;

	/** Reference to controlling actor (usually PlayerController) */
	UPROPERTY()
	class AActor* OwningActor;

	UPROPERTY()
	int32	MaxPacket;						// Maximum packet size.

	UPROPERTY()
	uint32 InternalAck:1;					// Internally ack all packets, for 100% reliable connections.

	struct FURL			URL;				// URL of the other side.

	// Track each type of bit used per-packet for bandwidth profiling

	/** Number of bits used for the packet id in the current packet. */
	int NumPacketIdBits;

	/** Number of bits used for bunches in the current packet. */
	int NumBunchBits;

	/** Number of bits used for acks in the current packet. */
	int NumAckBits;

	/** Number of bits used for padding in the current packet. */
	int NumPaddingBits;

	/** The maximum number of bits all packet handlers will reserve */
	int32 MaxPacketHandlerBits;

	/** Sets all of the bit-tracking variables to zero. */
	void ResetPacketBitCounts();

	/** What type of data is being written */
	enum class EWriteBitsDataType
	{
		Unknown,
		Bunch,
		Ack
	};

public:
	// Constants.
	enum{ MAX_CHANNELS         = 10240 };	// Maximum channels. TODO: This needs to differ per game somehow but cannot with shared executable

	// Connection information.

	EConnectionState	State;					// State this connection is in.
	
	uint32 bPendingDestroy:1;    // when true, playercontroller or beaconclient is being destroyed


	/** PacketHandler, for managing layered handler components, which modify packets as they are sent/received */
	TUniquePtr<PacketHandler> Handler;

	/** Reference to the PacketHandler component, for managing stateless connection handshakes */
	TWeakPtr<StatelessConnectHandlerComponent> StatelessConnectComponent;


	/** Whether this channel needs to byte swap all data or not */
	bool			bNeedsByteSwapping;
	/** Net id of remote player on this connection. Only valid on client connections (server side).*/
	UPROPERTY()
	FUniqueNetIdRepl PlayerId;

	// Negotiated parameters.
	int32			PacketOverhead;			// Bytes overhead per packet sent.
	FString			Challenge;				// Server-generated challenge.
	FString			ClientResponse;			// Client-generated response.
	int32			ResponseId;				// Id assigned by the server for linking responses to connections upon authentication
	FString			RequestURL;				// URL requested by client

	// Login state tracking
	EClientLoginState::Type	ClientLoginState;
	uint8					ExpectedClientLoginMsgType;	// Used to determine what the next expected control channel msg type should be from a connecting client

	// CD key authentication
	FString			CDKeyHash;				// Hash of client's CD key
	FString			CDKeyResponse;			// Client's response to CD key challenge

	// Internal.
	UPROPERTY()
	double			LastReceiveTime;		// Last time a packet was received, for timeout checking.
	double			LastReceiveRealtime;	// Last time a packet was received, using real time seconds (FPlatformTime::Seconds)
	double			LastGoodPacketRealtime;	// Last real time a packet was considered valid
	double			LastSendTime;			// Last time a packet was sent, for keepalives.
	double			LastTickTime;			// Last time of polling.
	int32			QueuedBits;			// Bits assumed to be queued up.
	int32			TickCount;				// Count of ticks.
	/** The last time an ack was received */
	float			LastRecvAckTime;
	/** Time when connection request was first initiated */
	float			ConnectTime;

	// Merge info.
	FBitWriterMark  LastStart;				// Most recently sent bunch start.
	FBitWriterMark  LastEnd;				// Most recently sent bunch end.
	bool			AllowMerge;				// Whether to allow merging.
	bool			TimeSensitive;			// Whether contents are time-sensitive.
	FOutBunch*		LastOutBunch;			// Most recent outgoing bunch.
	FOutBunch		LastOut;

	// Stat display.
	/** Time of last stat update */
	double			StatUpdateTime;
	/** Interval between gathering stats */
	float			StatPeriod;
	float			BestLag,   AvgLag;		// Lag.

	// Stat accumulators.
	float			LagAcc, BestLagAcc;		// Previous msec lag.
	int32			LagCount;				// Counter for lag measurement.
	double			LastTime, FrameTime;	// Monitors frame time.
	/** @todo document */
	double			CumulativeTime, AverageFrameTime; 
	/** @todo document */
	int32			CountedFrames;
	/** bytes sent/received on this connection (accumulated during a StatPeriod) */
	int32 InBytes, OutBytes;
	/** packets sent/received on this connection (accumulated during a StatPeriod) */
	int32 InPackets, OutPackets;
	/** bytes sent/received on this connection (per second) - these are from previous StatPeriod interval */
	int32 InBytesPerSecond, OutBytesPerSecond;
	/** packets sent/received on this connection (per second) - these are from previous StatPeriod interval */
	int32 InPacketsPerSecond, OutPacketsPerSecond;
	/** packets lost on this connection */
	int32 InPacketsLost, OutPacketsLost;

	// Packet.
	FBitWriter		SendBuffer;						// Queued up bits waiting to send
	double			OutLagTime[256];				// For lag measuring.
	int32			OutLagPacketId[256];			// For lag measuring.
	int32			OutBytesPerSecondHistory[256];	// For saturation measuring.
	float			RemoteSaturation;
	int32			InPacketId;						// Full incoming packet index.
	int32			OutPacketId;					// Most recently sent packet.
	int32 			OutAckPacketId;					// Most recently acked outgoing packet.

	bool			bLastHasServerFrameTime;

	// Channel table.
	class UChannel*		Channels		[ MAX_CHANNELS ];
	int32				OutReliable		[ MAX_CHANNELS ];
	int32				InReliable		[ MAX_CHANNELS ];
	int32				PendingOutRec	[ MAX_CHANNELS ];	// Outgoing reliable unacked data from previous (now destroyed) channel in this slot.  This contains the first chsequence not acked
	TArray<int32> QueuedAcks, ResendAcks;

	int32				InitOutReliable;
	int32				InitInReliable;

	// Network version
	uint32				EngineNetworkProtocolVersion;
	uint32				GameNetworkProtocolVersion;

	// Log tracking
	double			LogCallLastTime;
	int32			LogCallCount;
	int32			LogSustainedCount;

	/** @todo document */
	TMap<TWeakObjectPtr<AActor>, UActorChannel*, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<TWeakObjectPtr<AActor>, UActorChannel*>> ActorChannels;

	/** This holds a list of actor channels that want to fully shutdown, but need to continue processing bunches before doing so */
	TMap<FNetworkGUID, TArray<class UActorChannel*>> KeepProcessingActorChannelBunchesMap;

	/** A list of replicators that belong to recently dormant actors/objects */
	TMap< TWeakObjectPtr< UObject >, TSharedRef< FObjectReplicator > > DormantReplicatorMap;

	/** The server adds GUIDs to this set for each destroyed actor that does not have a channel
	 *  but that the client still knows about: startup, dormant, or recently dormant set.
	 *  This set is also populated from the UNetDriver for clients who join-in-progress, so that they can destroy any
	 *  startup actors that the server has already destroyed.
	 */
	TSet<FNetworkGUID>	DestroyedStartupOrDormantActors;

	/**
	 * on the server, the world the client has told us it has loaded
	 * used to make sure the client has traveled correctly, prevent replicating actors before level transitions are done, etc
	 */
	FName ClientWorldPackageName;

	/** 
	 * on the server, the package names of streaming levels that the client has told us it has made visible
	 * the server will only replicate references to Actors in visible levels so that it's impossible to send references to
	 * Actors the client has not initialized
	 */
	TArray<FName> ClientVisibleLevelNames;

#if DO_ENABLE_NET_TEST
	// For development.
	/** Packet settings for testing lag, net errors, etc */
	FPacketSimulationSettings PacketSimulationSettings;

	/** delayed packet array */
	TArray<DelayedPacket> Delayed;

	/** Copies the settings from the net driver to our local copy */
	void UpdatePacketSimulationSettings(void);
#endif

	/** 
	 * If true, will resend everything this connection has ever sent, since the connection has been open.
	 *	This functionality is used during replay checkpoints for example, so we can re-use the existing connection and channels to record
	 *	a version of each actor and capture all properties that have changed since the actor has been alive...
	 *	This will also act as if it needs to re-open all the channels, etc.
	 *   NOTE - This doesn't force all exports to happen again though, it will only export new stuff, so keep that in mind.
	 */
	bool bResendAllDataSinceOpen;


#if !UE_BUILD_SHIPPING
	/** Delegate for hooking ReceivedRawPacket */
	FOnReceivedRawPacket	ReceivedRawPacketDel;

	/** Delegate for hooking LowLevelSend */
	FOnLowLevelSend			LowLevelSendDel;
#endif


	/**
	 * Called to determine if a voice packet should be replicated to this
	 * connection or any of its child connections
	 *
	 * @param Sender - the sender of the voice packet
	 *
	 * @return true if it should be sent on this connection, false otherwise
	 */
	ENGINE_API bool ShouldReplicateVoicePacketFrom(const FUniqueNetId& Sender);
	
	/**
	 * @hack: set to net connection currently inside CleanUp(), for HasClientLoadedCurrentWorld() to be able to find it during PlayerController
	 * destruction, since we clear its Player before destroying it. (and that's not easily reversed)
	 */
	static class UNetConnection* GNetConnectionBeingCleanedUp;

	// Constructors and destructors.
	ENGINE_API UNetConnection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UObject Interface.

	ENGINE_API virtual void Serialize( FArchive& Ar ) override;

	ENGINE_API virtual void FinishDestroy() override;

	ENGINE_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/**
	 * Get the world the connection belongs to
	 *
	 * @return  Returns the world of the net driver, or the owning actor on this connection
	 */
	ENGINE_API virtual UWorld* GetWorld() const override;

	//~ End UObject Interface.


	//~ Begin FExec Interface.

	ENGINE_API virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) override;

	//~ End FExec Interface.

	/** read input */
	void ReadInput( float DeltaSeconds );

	/** 
	 * get the representation of a secondary splitscreen connection that reroutes calls to the parent connection
	 * @return NULL for this connection.
     */
	virtual UChildConnection* GetUChildConnection()
	{
		return NULL;
	}

	/** @return the remote machine address */
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort=false) PURE_VIRTUAL(UNetConnection::LowLevelGetRemoteAddress,return TEXT(""););

	/** @return the description of connection */
	virtual FString LowLevelDescribe() PURE_VIRTUAL(UNetConnection::LowLevelDescribe,return TEXT(""););

	/** Describe the connection. */
	ENGINE_API virtual FString Describe();

	/**
	 * Sends a byte stream to the remote endpoint using the underlying socket
	 *
	 * @param Data			The byte stream to send
	 * @param CountBytes	The length of the stream to send, in bytes
	 * @param CountBits		The length of the stream to send, in bits (to support bit-level additions to packets, from PacketHandler's)
	 */
	// @todo: Deprecate 'CountBytes' eventually
	ENGINE_API virtual void LowLevelSend(void* Data, int32 CountBytes, int32 CountBits)
		PURE_VIRTUAL(UNetConnection::LowLevelSend,);

	/** Validates the FBitWriter to make sure it's not in an error state */
	ENGINE_API virtual void ValidateSendBuffer();

	/** Resets the FBitWriter to its default state */
	ENGINE_API virtual void InitSendBuffer();

	/** Make sure this connection is in a reasonable state. */
	ENGINE_API virtual void AssertValid();

	/** Send an acknowledgment. */
	ENGINE_API virtual void SendAck( int32 PacketId, bool FirstTime=1);

	/**
	 * flushes any pending data, bundling it into a packet and sending it via LowLevelSend()
	 * also handles network simulation settings (simulated lag, packet loss, etc) unless bIgnoreSimulation is true
	 */
	ENGINE_API virtual void FlushNet(bool bIgnoreSimulation = false);

	/** Poll the connection. If it is timed out, close it. */
	ENGINE_API virtual void Tick();

	/** Return whether this channel is ready for sending. */
	ENGINE_API virtual int32 IsNetReady( bool Saturate );

	/** 
     * Handle the player controller client
     *
     * @param PC player controller for this player
     * @param NetConnection the connection the player is communicating on
     */
	ENGINE_API virtual void HandleClientPlayer( class APlayerController* PC, class UNetConnection* NetConnection );

	/** @return the address of the connection as an integer */
	virtual int32 GetAddrAsInt(void)
	{
		return 0;
	}

	/** @return the port of the connection as an integer */
	virtual int32 GetAddrPort(void)
	{
		return 0;
	}

	/** closes the connection (including sending a close notify across the network) */
	ENGINE_API void Close();

	/** closes the control channel, cleans up structures, and prepares for deletion */
	ENGINE_API virtual void CleanUp();

	/**
	 * Initialize common settings for this connection instance
	 *
	 * @param InDriver the net driver associated with this connection
	 * @param InSocket the socket associated with this connection
	 * @param InURL the URL to init with
	 * @param InState the connection state to start with for this connection
	 * @param InMaxPacket the max packet size that will be used for sending
	 * @param InPacketOverhead the packet overhead for this connection type
	 */
	ENGINE_API virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0);

	/**
	 * Initialize this connection instance *from* a remote source
	 *
	 * @param InDriver the net driver associated with this connection
	 * @param InSocket the socket associated with this connection
	 * @param InURL the URL to init with
	 * @param InRemoteAddr the remote address for this connection
	 * @param InState the connection state to start with for this connection
	 * @param InMaxPacket the max packet size that will be used for sending
	 * @param InPacketOverhead the packet overhead for this connection type
	 */
	ENGINE_API virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) PURE_VIRTUAL(UNetConnection::InitRemoteConnection, );
	
	/**
	 * Initialize this connection instance *to* a remote source
	 *
	 * @param InDriver the net driver associated with this connection
	 * @param InSocket the socket associated with this connection
	 * @param InURL the URL to init with
	 * @param InRemoteAddr the remote address for this connection
	 * @param InState the connection state to start with for this connection
	 * @param InMaxPacket the max packet size that will be used for sending
	 * @param InPacketOverhead the packet overhead for this connection type
	 */
	ENGINE_API virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) PURE_VIRTUAL(UNetConnection::InitLocalConnection, );
	
	/**
	 * Initializes an "addressless" connection with the passed in settings
	 *
	 * @param InDriver the net driver associated with this connection
	 * @param InState the connection state to start with for this connection
	 * @param InURL the URL to init with
	 * @param InConnectionSpeed Optional connection speed override
	 */
	ENGINE_API virtual void InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed=0, int32 InMaxPacket=0);


	/**
	 * Initializes the PacketHandler
	 */
	ENGINE_API virtual void InitHandler();

	/**
	 * Initializes the sequence numbers for the connection, usually from shared randomized data
	 *
	 * @param IncomingSequence	The initial sequence number for incoming packets
	 * @param OutgoingSequence	The initial sequence number for outgoing packets
	 */
	ENGINE_API virtual void InitSequence(int32 IncomingSequence, int32 OutgoingSequence);

	/**
	 * Sets the encryption key and enables encryption.
	 */
	ENGINE_API void EnableEncryptionWithKey(TArrayView<const uint8> Key);

	/**
	 * Sets the encryption key, enables encryption, and sends the encryption ack to the client.
	 */
	ENGINE_API void EnableEncryptionWithKeyServer(TArrayView<const uint8> Key);

	/** 
	* Gets a unique ID for the connection, this ID depends on the underlying connection
	* For IP connections this is an IP Address and port, for steam this is a SteamID
	*/
	ENGINE_API virtual FString RemoteAddressToString() PURE_VIRTUAL(UNetConnection::RemoteAddressToString, return TEXT("Error"););
	
	
	/** Called by UActorChannel. Handles creating a new replicator for an actor */
	ENGINE_API virtual TSharedPtr<FObjectReplicator> CreateReplicatorForNewActorChannel(UObject* Object);

	// Functions.

	/** Resend any pending acks. */
	void PurgeAcks();

	/** Send package map to the remote. */
	void SendPackageMap();

	/** 
	 * Appends the passed in data to the SendBuffer to be sent when FlushNet is called
	 * @param Bits Data as bits to be appended to the send buffer
	 * @param SizeInBits Number of bits to append
	 * @param ExtraBits (optional) Second set of bits to be appended to the send buffer that need to send with the first set of bits
	 * @param ExtraSizeInBits (optional) Number of secondary bits to append
	 * @param TypeOfBits (optional) The type of data being written, for profiling and bandwidth tracking purposes
	 */
	int32 WriteBitsToSendBuffer( 
		const uint8 *	Bits, 
		const int32		SizeInBits, 
		const uint8 *	ExtraBits = NULL, 
		const int32		ExtraSizeInBits = 0,
		EWriteBitsDataType DataType =  EWriteBitsDataType::Unknown);

	/** Returns number of bits left in current packet that can be used without causing a flush.  */
	int64 GetFreeSendBufferBits();

	/** Pops the LastStart bits off of the send buffer, used for merging bunches */
	void PopLastStart();

	/** 
	 * returns whether the client has initialized the level required for the given object
	 * @return true if the client has initialized the level the object is in or the object is not in a level, false otherwise
	 */
	ENGINE_API virtual bool ClientHasInitializedLevelFor(const UObject* TestObject) const;

	/**
	 * Allows the connection to process the raw data that was received
	 *
	 * @param Data the data to process
	 * @param Count the size of the data buffer to process
	 */
	ENGINE_API virtual void ReceivedRawPacket(void* Data,int32 Count);

	/** Send a raw bunch. */
	ENGINE_API int32 SendRawBunch( FOutBunch& Bunch, bool InAllowMerge );

	/** The maximum number of bits allowed within a single bunch. */
	FORCEINLINE int32 GetMaxSingleBunchSizeBits() const
	{
		return (MaxPacket * 8) - MAX_BUNCH_HEADER_BITS - MAX_PACKET_TRAILER_BITS - MAX_PACKET_HEADER_BITS - MaxPacketHandlerBits;
	}

	/** @return The driver object */
	UNetDriver* GetDriver() {return Driver;}

	/** @todo document */
	class UControlChannel* GetControlChannel();

	/** Create a channel. */
	ENGINE_API UChannel* CreateChannel( EChannelType Type, bool bOpenedLocally, int32 ChannelIndex=INDEX_NONE );

	/** Handle a packet we just received. */
	void ReceivedPacket( FBitReader& Reader );

	/** Packet was negatively acknowledged. */
	void ReceivedNak( int32 NakPacketId );

	/** Clear all Game specific state. Called during seamless travel */
	void ResetGameWorldState();

	/** Make sure this connection is in a reasonable state. */
	void SlowAssertValid()
	{
#if DO_GUARD_SLOW
		AssertValid();
#endif
	}

	/**
	 * @return Finds the voice channel for this connection or NULL if none
	 */
	ENGINE_API class UVoiceChannel* GetVoiceChannel();

	ENGINE_API virtual void FlushDormancy(class AActor* Actor);

	/** Forces properties on this actor to do a compare for one frame (rather than share shadow state) */
	ENGINE_API void ForcePropertyCompare( AActor* Actor );

	/** Wrapper for validating an objects dormancy state, and to prepare the object for replication again */
	void FlushDormancyForObject( UObject* Object );

	/** Wrapper for setting the current client login state, so we can trap for debugging, and verbosity purposes. */
	ENGINE_API void SetClientLoginState( const EClientLoginState::Type NewState );

	/** Wrapper for setting the current expected client login msg type. */
	ENGINE_API void SetExpectedClientLoginMsgType( const uint8 NewType );

	/** This function validates that ClientMsgType is the next expected msg type. */
	ENGINE_API bool IsClientMsgTypeValid( const uint8 ClientMsgType );

	/**
	 * This function tracks the number of log calls per second for this client, 
	 * and disconnects the client if it detects too many calls are made per second
	 */
	ENGINE_API bool TrackLogsPerSecond();

	/**
	* Return current timeout value that should be used
	*/
	ENGINE_API float GetTimeoutValue();

	/** Adds the channel to the ticking channels list. USed to selectively tick channels that have queued bunches or are pending dormancy. */
	void StartTickingChannel(UChannel* Channel) { ChannelsToTick.AddUnique(Channel); }

	/** Removes a channel from the ticking list directly */
	void StopTickingChannel(UChannel* Channel) { ChannelsToTick.Remove(Channel); }

	FORCEINLINE FHistogram GetNetHistogram() const { return NetConnectionHistogram; }

	/** Whether or not a client packet has been received - used serverside, to delay any packet sends */
	FORCEINLINE bool HasReceivedClientPacket()
	{
		return !!InternalAck || Driver->ServerConnection != nullptr || InReliable[0] != InitInReliable;
	}

	/**
	 * Sets the PlayerOnlinePlatformName member.
	 * Called by the engine during the login process with the NMT_Login message parameter.
	 */
	void SetPlayerOnlinePlatformName(const FName InPlayerOnlinePlatformName);

	/** Returns the online platform name for the player on this connection. Only valid for client connections on servers. */
	ENGINE_API FName GetPlayerOnlinePlatformName() const { return PlayerOnlinePlatformName; }

protected:

	void CleanupDormantActorState();

private:
	/**
	 * The channels that need ticking. This will be a subset of OpenChannels, only including
	 * channels that need to process either dormancy or queued bunches. Should be a significant
	 * optimization over ticking and calling virtual functions on the potentially hundreds of
	 * OpenChannels every frame.
	 */
	UPROPERTY()
	TArray<UChannel*> ChannelsToTick;

	/** Histogram of the received packet time */
	FHistogram NetConnectionHistogram;

	/** Online platform ID of remote player on this connection. Only valid on client connections (server side).*/
	FName PlayerOnlinePlatformName;
};



/** Help structs for temporarily setting network settings */
struct FNetConnectionSettings
{
	FNetConnectionSettings( UNetConnection* InConnection )
	{
#if DO_ENABLE_NET_TEST
		PacketLag = InConnection->PacketSimulationSettings.PktLag;
#else
		PacketLag = 0;
#endif
	}

	FNetConnectionSettings( int32 InPacketLag )
	{
		PacketLag = InPacketLag;
	}

	void ApplyTo(UNetConnection* Connection)
	{
#if DO_ENABLE_NET_TEST
		Connection->PacketSimulationSettings.PktLag = PacketLag;
#endif
	}

	int32 PacketLag;
};

/** Allows you to temporarily set connection settings within a scape. This will also force flush the connection before/after.
 *	Lets you do things like force a single channel to delay or drop packets
 */
struct FScopedNetConnectionSettings
{
	FScopedNetConnectionSettings(UNetConnection* InConnection, FNetConnectionSettings NewSettings, bool Apply=true) : 
		Connection(InConnection), OldSettings(InConnection), ShouldApply(Apply)
	{
		if (ShouldApply)
		{
			Connection->FlushNet();
			NewSettings.ApplyTo(Connection);
		}
	}
	~FScopedNetConnectionSettings()
	{
		if (ShouldApply)
		{
			Connection->FlushNet();
			OldSettings.ApplyTo(Connection);
		}
	}

	UNetConnection * Connection;
	FNetConnectionSettings OldSettings;
	bool ShouldApply;
};
