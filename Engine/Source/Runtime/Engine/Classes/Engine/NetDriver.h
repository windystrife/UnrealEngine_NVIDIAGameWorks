// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
// Base class of a network driver attached to an active or pending level.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Misc/NetworkGuid.h"
#include "UObject/CoreNet.h"
#include "GameFramework/WorldSettings.h"
#include "PacketHandler.h"
#include "Channel.h"

#include "NetDriver.generated.h"

class Error;
class FNetGUIDCache;
class FNetworkNotify;
class FNetworkObjectList;
class FObjectReplicator;
class FRepChangedPropertyTracker;
class FRepLayout;
class FReplicationChangelistMgr;
class FVoicePacket;
class StatelessConnectHandlerComponent;
class UNetConnection;
struct FNetworkObjectInfo;

extern ENGINE_API TAutoConsoleVariable<int32> CVarNetAllowEncryption;

// Delegates

#if !UE_BUILD_SHIPPING
/**
 * Delegate for hooking ProcessRemoteFunction (used by NetcodeUnitTest)
 *
 * @param Actor				The actor the RPC will be called in
 * @param Function			The RPC to call
 * @param Parameters		The parameters data blob
 * @param OutParms			Out parameter information (irrelevant for RPC's)
 * @param Stack				The script stack
 * @param SubObject			The sub-object the RPC is being called in (if applicable)
 * @param bBlockSendRPC		Whether or not to block sending of the RPC (defaults to false)
 */
DECLARE_DELEGATE_SevenParams(FOnSendRPC, AActor* /*Actor*/, UFunction* /*Function*/, void* /*Parameters*/,
									FOutParmRec* /*OutParms*/, FFrame* /*Stack*/, UObject* /*SubObject*/, bool& /*bBlockSendRPC*/);
#endif

//
// Whether to support net lag and packet loss testing.
//
#define DO_ENABLE_NET_TEST !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


/** Holds the packet simulation settings in one place */
USTRUCT()
struct ENGINE_API FPacketSimulationSettings
{
	GENERATED_USTRUCT_BODY()

	/**
	 * When set, will cause calls to FlushNet to drop packets.
	 * Value is treated as % of packets dropped (i.e. 0 = None, 100 = All).
	 * No general pattern / ordering is guaranteed.
	 * Clamped between 0 and 100.
	 *
	 * Works with all other settings.
	 */
	UPROPERTY(EditAnywhere, Category="Simulation Settings")
	int32	PktLoss;

	/**
	 * When set, will cause calls to FlushNet to change ordering of packets at random.
	 * Value is treated as a bool (i.e. 0 = False, anything else = True).
	 * This works by randomly selecting packets to be delayed until a subsequent call to FlushNet.
	 *
	 * Takes precedence over PktDup and PktLag.
	 */
	UPROPERTY(EditAnywhere, Category="Simulation Settings")
	int32	PktOrder;

	/**
	 * When set, will cause calls to FlushNet to duplicate packets.
	 * Value is treated as % of packets duplicated (i.e. 0 = None, 100 = All).
	 * No general pattern / ordering is guaranteed.
	 * Clamped between 0 and 100.
	 *
	 * Cannot be used with PktOrder or PktLag.
	 */
	UPROPERTY(EditAnywhere, Category="Simulation Settings")
	int32	PktDup;
	
	/**
	 * When set, will cause calls to FlushNet to delay packets.
	 * Value is treated as millisecond lag.
	 *
	 * Cannot be used with PktOrder.
	 */
	UPROPERTY(EditAnywhere, Category="Simulation Settings")
	int32	PktLag;
	
	/**
	 * When set, will cause PktLag to use variable lag instead of constant.
	 * Value is treated as millisecond lag range (e.g. -GivenVariance <= 0 <= GivenVariance).
	 * Clamped between 0 and 100.
	 *
	 * Can only be used when PktLag is enabled.
	 */
	UPROPERTY(EditAnywhere, Category="Simulation Settings")
	int32	PktLagVariance;

	/** Ctor. Zeroes the settings */
	FPacketSimulationSettings() : 
		PktLoss(0),
		PktOrder(0),
		PktDup(0),
		PktLag(0),
		PktLagVariance(0) 
	{
	}

	/** reads in settings from the .ini file 
	 * @note: overwrites all previous settings
	 */
	void LoadConfig(const TCHAR* OptionalQualifier = nullptr);

	/**
	 * Registers commands for auto-completion, etc.
	 */
	void RegisterCommands();

	/**
	 * Unregisters commands for auto-completion, etc.
	 */
	void UnregisterCommands();

	/**
	 * Reads the settings from a string: command line or an exec
	 *
	 * @param Stream the string to read the settings from
	 * @Param OptionalQualifier: optional string to prepend to Pkt* settings. E.g, "GameNetDriverPktLoss=50"
	 */
	bool ParseSettings(const TCHAR* Stream, const TCHAR* OptionalQualifier=nullptr);

	bool ParseHelper(const TCHAR* Cmd, const TCHAR* Name, int32& Value, const TCHAR* OptionalQualifier);

	bool ConfigHelperInt(const TCHAR* Name, int32& Value, const TCHAR* OptionalQualifier);
	bool ConfigHelperBool(const TCHAR* Name, bool& Value, const TCHAR* OptionalQualifier);
};

//
// Priority sortable list.
//
struct FActorPriority
{
	int32						Priority;	// Update priority, higher = more important.
	
	FNetworkObjectInfo*			ActorInfo;	// Actor info.
	class UActorChannel*		Channel;	// Actor channel.

	struct FActorDestructionInfo *	DestructionInfo;	// Destroy an actor

	FActorPriority() : 
		Priority(0), ActorInfo(NULL), Channel(NULL), DestructionInfo(NULL)
	{}

	FActorPriority(class UNetConnection* InConnection, class UActorChannel* InChannel, FNetworkObjectInfo* InActorInfo, const TArray<struct FNetViewer>& Viewers, bool bLowBandwidth);
	FActorPriority(class UNetConnection* InConnection, struct FActorDestructionInfo * DestructInfo, const TArray<struct FNetViewer>& Viewers );
};

struct FCompareFActorPriority
{
	FORCEINLINE bool operator()( const FActorPriority& A, const FActorPriority& B ) const
	{
		return B.Priority < A.Priority;
	}
};

struct FActorDestructionInfo
{
	TWeakObjectPtr<ULevel>		Level;
	TWeakObjectPtr<UObject>		ObjOuter;
	FVector			DestroyedPosition;
	FNetworkGUID	NetGUID;
	FString			PathName;

	FName			StreamingLevelName;
};


UCLASS(Abstract, customConstructor, transient, MinimalAPI, config=Engine)
class UNetDriver : public UObject, public FExec
{
	GENERATED_UCLASS_BODY()

protected:

	ENGINE_API void InternalProcessRemoteFunction(class AActor* Actor, class UObject* SubObject, class UNetConnection* Connection, class UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack, bool IsServer);

public:

	/** Used to specify the class to use for connections */
	UPROPERTY(Config)
	FString NetConnectionClassName;

	/** @todo document */
	UPROPERTY(Config)
	int32 MaxDownloadSize;

	/** @todo document */
	UPROPERTY(Config)
	uint32 bClampListenServerTickRate:1;

	/** @todo document */
	UPROPERTY(Config)
	int32 NetServerMaxTickRate;

	/** @todo document */
	UPROPERTY(Config)
	int32 MaxInternetClientRate;

	/** @todo document */
	UPROPERTY(Config)
	int32 MaxClientRate;

	/** Amount of time a server will wait before traveling to next map, gives clients time to receive final RPCs on existing level @see NextSwitchCountdown */
	UPROPERTY(Config)
	float ServerTravelPause;

	/** @todo document */
	UPROPERTY(Config)
	float SpawnPrioritySeconds;

	/** @todo document */
	UPROPERTY(Config)
	float RelevantTimeout;

	/** @todo document */
	UPROPERTY(Config)
	float KeepAliveTime;

	/** Amount of time to wait for a new net connection to be established before destroying the connection */
	UPROPERTY(Config)
	float InitialConnectTimeout;

	/** 
	 * Amount of time to wait before considering an established connection timed out.  
	 * Typically shorter than the time to wait on a new connection because this connection
	 * should already have been setup and any interruption should be trapped quicker.
	 */
	UPROPERTY(Config)
	float ConnectionTimeout;

	/**
	* A multiplier that is applied to the above values when we are running with unoptimized builds (debug)
	* or data (uncooked). This allows us to retain normal timeout behavior while debugging without resorting
	* to the nuclear 'notimeouts' option or bumping the values above. If ==0 multiplier = 1
	*/
	UPROPERTY(Config)
	float TimeoutMultiplierForUnoptimizedBuilds;

	/**
	 * If true, ignore timeouts completely.  Should be used only in development
	 */
	UPROPERTY(Config)
	bool bNoTimeouts;

	/** Connection to the server (this net driver is a client) */
	UPROPERTY()
	class UNetConnection* ServerConnection;

	/** Array of connections to clients (this net driver is a host) */
	UPROPERTY()
	TArray<class UNetConnection*> ClientConnections;


	/** Serverside PacketHandler for managing connectionless packets */
	TUniquePtr<PacketHandler> ConnectionlessHandler;

	/** Reference to the PacketHandler component, for managing stateless connection handshakes */
	TWeakPtr<StatelessConnectHandlerComponent> StatelessConnectComponent;


	/** World this net driver is associated with */
	UPROPERTY()
	class UWorld* World;

	/** @todo document */
	TSharedPtr< class FNetGUIDCache > GuidCache;

	TSharedPtr< class FClassNetCacheMgr >	NetCache;

	/** The loaded UClass of the net connection type to use */
	UPROPERTY()
	UClass* NetConnectionClass;

	/** @todo document */
	UPROPERTY()
	UProperty* RoleProperty;
	
	/** @todo document */
	UPROPERTY()
	UProperty* RemoteRoleProperty;

	/** Used to specify the net driver to filter actors with (NAME_None || NAME_GameNetDriver is the default net driver) */
	UPROPERTY(Config)
	FName NetDriverName;

	/** The UChannel classes that should be used under this net driver */
	UClass* ChannelClasses[CHTYPE_MAX];
	
	/** @return true if the specified channel type exists. */
	FORCEINLINE bool IsKnownChannelType(int32 Type)
	{
		return Type >= 0 && Type < CHTYPE_MAX && ChannelClasses[Type] != nullptr;
	}

	/** Change the NetDriver's NetDriverName. This will also reinit packet simulation settings so that settings can be qualified to a specific driver. */
	void SetNetDriverName(FName NewNetDriverNamed);

	void InitPacketSimulationSettings();

	/** Interface for communication network state to others (ie World usually, but anything that implements FNetworkNotify) */
	class FNetworkNotify*		Notify;
	
	/** Accumulated time for the net driver, updated by Tick */
	UPROPERTY()
	float						Time;

	/** Last realtime a tick dispatch occurred. Used currently to try and diagnose timeout issues */
	double						LastTickDispatchRealtime;

	/** If true then client connections are to other client peers */
	bool						bIsPeer;
	/** @todo document */
	bool						ProfileStats;
	/** Timings for Socket::SendTo() and Socket::RecvFrom() */
	int32						SendCycles, RecvCycles;
	/** Stats for network perf */
	uint32						InBytesPerSecond;
	/** todo document */
	uint32						OutBytesPerSecond;
	/** todo document */
	uint32						InBytes;
	/** todo document */
	uint32						OutBytes;
	/** Outgoing rate of NetGUID Bunches */
	uint32						NetGUIDOutBytes;
	/** Incoming rate of NetGUID Bunches */
	uint32						NetGUIDInBytes;
	/** todo document */
	uint32						InPackets;
	/** todo document */
	uint32						OutPackets;
	/** todo document */
	uint32						InBunches;
	/** todo document */
	uint32						OutBunches;
	/** todo document */
	uint32						InPacketsLost;
	/** todo document */
	uint32						OutPacketsLost;
	/** todo document */
	uint32						InOutOfOrderPackets;
	/** todo document */
	uint32						OutOutOfOrderPackets;
	/** Tracks the total number of voice packets sent */
	uint32						VoicePacketsSent;
	/** Tracks the total number of voice bytes sent */
	uint32						VoiceBytesSent;
	/** Tracks the total number of voice packets received */
	uint32						VoicePacketsRecv;
	/** Tracks the total number of voice bytes received */
	uint32						VoiceBytesRecv;
	/** Tracks the voice data percentage of in bound bytes */
	uint32						VoiceInPercent;
	/** Tracks the voice data percentage of out bound bytes */
	uint32						VoiceOutPercent;
	/** Time of last stat update */
	double						StatUpdateTime;
	/** Interval between gathering stats */
	float						StatPeriod;
	/** Collect net stats even if not FThreadStats::IsCollectingData(). */
	bool bCollectNetStats;
	/** Time of last netdriver cleanup pass */
	double						LastCleanupTime;
	/** Used to determine if checking for standby cheats should occur */
	bool						bIsStandbyCheckingEnabled;
	/** Used to determine whether we've already caught a cheat or not */
	bool						bHasStandbyCheatTriggered;
	/** The amount of time without packets before triggering the cheat code */
	float						StandbyRxCheatTime;
	/** todo document */
	float						StandbyTxCheatTime;
	/** The point we think the host is cheating or shouldn't be hosting due to crappy network */
	int32						BadPingThreshold;
	/** The number of clients missing data before triggering the standby code */
	float						PercentMissingForRxStandby;
	float						PercentMissingForTxStandby;
	/** The number of clients with bad ping before triggering the standby code */
	float						PercentForBadPing;
	/** The amount of time to wait before checking a connection for standby issues */
	float						JoinInProgressStandbyWaitTime;
	/** Used to track whether a given actor was replicated by the net driver recently */
	int32						NetTag;
	/** Dumps next net update's relevant actors when true*/
	bool						DebugRelevantActors;

	TArray< TWeakObjectPtr<AActor> >	LastPrioritizedActors;
	TArray< TWeakObjectPtr<AActor> >	LastRelevantActors;
	TArray< TWeakObjectPtr<AActor> >	LastSentActors;
	TArray< TWeakObjectPtr<AActor> >	LastNonRelevantActors;

	void						PrintDebugRelevantActors();
	
	/** The server adds an entry into this map for every actor that is destroyed that join-in-progress
	 *  clients need to know about, that is, startup actors. Also, individual UNetConnections
	 *  need to keep track of FActorDestructionInfo for dormant and recently-dormant actors in addition
	 *  to startup actors (because they won't have an associated channel), and this map stores those
	 *  FActorDestructionInfos also.
	 */
	TMap<FNetworkGUID, FActorDestructionInfo>	DestroyedStartupOrDormantActors;

	/** Maps FRepChangedPropertyTracker to active objects that are replicating properties */
	TMap< TWeakObjectPtr< UObject >, TSharedPtr< FRepChangedPropertyTracker > >	RepChangedPropertyTrackerMap;
	/** Used to invalidate properties marked "unchanged" in FRepChangedPropertyTracker's */
	uint32																		ReplicationFrame;

	/** Maps FRepLayout to the respective UClass */
	TMap< TWeakObjectPtr< UObject >, TSharedPtr< FRepLayout > >					RepLayoutMap;

	/** Maps an object to the respective FReplicationChangelistMgr */
	TMap< TWeakObjectPtr< UObject >, TSharedPtr< FReplicationChangelistMgr > >	ReplicationChangeListMap;

	/** Creates if necessary, and returns a FRepLayout that maps to the passed in UClass */
	TSharedPtr< FRepLayout >	GetObjectClassRepLayout( UClass * InClass );

	/** Creates if necessary, and returns a FRepLayout that maps to the passed in UFunction */
	TSharedPtr<FRepLayout>		GetFunctionRepLayout( UFunction * Function );

	/** Creates if necessary, and returns a FRepLayout that maps to the passed in UStruct */
	TSharedPtr<FRepLayout>		GetStructRepLayout( UStruct * Struct );

	/** Returns the FReplicationChangelistMgr that is associated with the passed in object */
	TSharedPtr< FReplicationChangelistMgr > GetReplicationChangeListMgr( UObject* Object );

	TMap< FNetworkGUID, TSet< FObjectReplicator* > >	GuidToReplicatorMap;
	int32												TotalTrackedGuidMemoryBytes;
	TSet< FObjectReplicator* >							UnmappedReplicators;

	/** Handles to various registered delegates */
	FDelegateHandle TickDispatchDelegateHandle;
	FDelegateHandle TickFlushDelegateHandle;
	FDelegateHandle PostTickFlushDelegateHandle;

#if !UE_BUILD_SHIPPING
	/** Delegate for hooking ProcessRemoteFunction */
	FOnSendRPC	SendRPCDel;
#endif

	/** Tracks the amount of time spent during the current frame processing queued bunches. */
	float ProcessQueuedBunchesCurrentFrameMilliseconds;

	/**
	* Updates the standby cheat information and
	 * causes the dialog to be shown/hidden as needed
	 */
	void UpdateStandbyCheatStatus(void);

#if DO_ENABLE_NET_TEST
	FPacketSimulationSettings	PacketSimulationSettings;

	ENGINE_API void SetPacketSimulationSettings(FPacketSimulationSettings NewSettings);
#endif

	// Constructors.
	ENGINE_API UNetDriver(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	//~ Begin UObject Interface.
	ENGINE_API virtual void PostInitProperties() override;
	ENGINE_API virtual void FinishDestroy() override;
	ENGINE_API virtual void Serialize( FArchive& Ar ) override;
	ENGINE_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.

	//~ Begin FExec Interface

	/**
	 * Handle exec commands
	 *
	 * @param InWorld	the world context
	 * @param Cmd		the exec command being executed
	 * @param Ar		the archive to log results to
	 *
	 * @return true if the handler consumed the input, false to continue searching handlers
	 */
	ENGINE_API virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog) override;

	ENGINE_API ENetMode	GetNetMode() const;

	//~ End FExec Interface.

	/** 
	 * Returns true if this net driver is valid for the current configuration.
	 * Safe to call on a CDO if necessary
	 *
	 * @return true if available, false otherwise
	 */
	ENGINE_API virtual bool IsAvailable() const PURE_VIRTUAL( UNetDriver::IsAvailable, return false;)

	/**
	 * Common initialization between server and client connection setup
	 * 
	 * @param bInitAsClient are we a client or server
	 * @param InNotify notification object to associate with the net driver
	 * @param URL destination
	 * @param bReuseAddressAndPort whether to allow multiple sockets to be bound to the same address/port
	 * @param Error output containing an error string on failure
	 *
	 * @return true if successful, false otherwise (check Error parameter)
	 */
	ENGINE_API virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error);

	/**
	 * Initialize the net driver in client mode
	 *
	 * @param InNotify notification object to associate with the net driver
	 * @param ConnectURL remote ip:port of host to connect to
	 * @param Error resulting error string from connection attempt
	 * 
	 * @return true if successful, false otherwise (check Error parameter)
	 */
	ENGINE_API virtual bool InitConnect(class FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error ) PURE_VIRTUAL( UNetDriver::InitConnect, return true;);

	/**
	 * Initialize the network driver in server mode (listener)
	 *
	 * @param InNotify notification object to associate with the net driver
	 * @param ListenURL the connection URL for this listener
	 * @param bReuseAddressAndPort whether to allow multiple sockets to be bound to the same address/port
	 * @param Error out param with any error messages generated 
	 *
	 * @return true if successful, false otherwise (check Error parameter)
	 */
	ENGINE_API virtual bool InitListen(class FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error) PURE_VIRTUAL( UNetDriver::InitListen, return true;);

	/**
	 * Initialize a PacketHandler for serverside net drivers, for handling connectionless packets
	 * NOTE: Only triggered by net driver subclasses that support it - from within InitListen.
	 */
	ENGINE_API virtual void InitConnectionlessHandler();

	/**
	 * Flushes all packets queued by the connectionless PacketHandler
	 * NOTE: This should be called shortly after all calls to PacketHandler::IncomingConnectionless, to minimize packet buffer buildup.
	 */
	ENGINE_API virtual void FlushHandler();


	/** Initializes the net connection class to use for new connections */
	ENGINE_API virtual bool InitConnectionClass(void);

	/** Shutdown all connections managed by this net driver */
	ENGINE_API virtual void Shutdown();

	/* Close socket and Free the memory the OS allocated for this socket */
	ENGINE_API virtual void LowLevelDestroy();

	/* @return network number */
	virtual FString LowLevelGetNetworkNumber() PURE_VIRTUAL(UNetDriver::LowLevelGetNetworkNumber,return TEXT(""););

	/** Make sure this connection is in a reasonable state. */
	ENGINE_API virtual void AssertValid();

	/**
	 * Called to replicate any relevant actors to the connections contained within this net driver
	 *
	 * Process as many clients as allowed given Engine.NetClientTicksPerSecond, first building a list of actors to consider for relevancy checking,
	 * and then attempting to replicate each actor for each connection that it is relevant to until the connection becomes saturated.
	 *
	 * NetClientTicksPerSecond is used to throttle how many clients are updated each frame, hoping to avoid saturating the server's upstream bandwidth, although
	 * the current solution is far from optimal.  Ideally the throttling could be based upon the server connection becoming saturated, at which point each
	 * connection is reduced to priority only updates, and spread out amongst several ticks.  Also might want to investigate eliminating the redundant consider/relevancy
	 * checks for Actors that were successfully replicated for some channels but not all, since that would make a decent CPU optimization.
	 *
	 * @param DeltaSeconds elapsed time since last call
	 *
	 * @return the number of actors that were replicated
	 */
	ENGINE_API virtual int32 ServerReplicateActors(float DeltaSeconds);

	/**
	 * Process a remote function call on some actor destined for a remote location
	 *
	 * @param Actor actor making the function call
	 * @param Function function definition called
	 * @param Params parameters in a UObject memory layout
	 * @param Stack stack frame the UFunction is called in
	 * @param SubObject optional: sub object to actually call function on
	 */
	ENGINE_API virtual void ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = NULL ) PURE_VIRTUAL(UNetDriver::ProcessRemoteFunction,);

	/** handle time update */
	ENGINE_API virtual void TickDispatch( float DeltaTime );

	/** ReplicateActors and Flush */
	ENGINE_API virtual void TickFlush(float DeltaSeconds);

	/** PostTick actions */
	ENGINE_API virtual void PostTickFlush();


	/**
	 * Sends a 'connectionless' (not associated with a UNetConection) packet, to the specified address.
	 * NOTE: Address is an abstract format defined by subclasses. Anything calling this, must use an address supplied by the net driver.
	 *
	 * @param Address		The address the packet should be sent to (format is abstract, determined by net driver subclasses)
	 * @param Data			The packet data
	 * @param CountBits		The size of the packet data, in bits
	 */
	ENGINE_API virtual void LowLevelSend(FString Address, void* Data, int32 CountBits)
		PURE_VIRTUAL(UNetDriver::LowLevelSend,);

	/**
	 * Process any local talker packets that need to be sent to clients
	 */
	ENGINE_API virtual void ProcessLocalServerPackets();

	/**
	 * Process any local talker packets that need to be sent to the server
	 */
	ENGINE_API virtual void ProcessLocalClientPackets();

	/**
	 * Update the LagState based on a heuristic to determine if we are network lagging
	 */
	ENGINE_API virtual void UpdateNetworkLagState();

	/**
	 * Determines which other connections should receive the voice packet and
	 * queues the packet for those connections. Used for sending both local/remote voice packets.
	 *
	 * @param VoicePacket the packet to be queued
	 * @param CameFromConn the connection this packet came from (NULL if local)
	 */
	ENGINE_API virtual void ReplicateVoicePacket(TSharedPtr<class FVoicePacket> VoicePacket, class UNetConnection* CameFromConn);

#if !UE_BUILD_SHIPPING
	/**
	 * Exec command handlers
	 */
	bool HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePackageMapCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetFloodCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetDebugTextCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetDisconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNetDumpServerRPCCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif

	/** Flushes actor from NetDriver's dormancy list, but does not change any state on the Actor itself */
	ENGINE_API void FlushActorDormancy(class AActor *Actor);

	/** Forces properties on this actor to do a compare for one frame (rather than share shadow state) */
	ENGINE_API void ForcePropertyCompare( AActor* Actor );

	/** Force this actor to be relevant for at least one update */
	ENGINE_API void ForceActorRelevantNextUpdate(AActor* Actor);

	/** Called when a spawned actor is destroyed. */
	ENGINE_API virtual void NotifyActorDestroyed( AActor* Actor, bool IsSeamlessTravel=false );

	ENGINE_API virtual void NotifyStreamingLevelUnload( ULevel* );

	ENGINE_API virtual void NotifyActorLevelUnloaded( AActor* Actor );
	
	/** creates a child connection and adds it to the given parent connection */
	ENGINE_API virtual class UChildConnection* CreateChild(UNetConnection* Parent);

	/** @return String that uniquely describes the net driver instance */
	FString GetDescription() 
	{ 
		return FString::Printf(TEXT("%s %s%s"), *NetDriverName.ToString(), *GetName(), bIsPeer ? TEXT("(PEER)") : TEXT(""));
	}

	/** @return true if this netdriver is handling accepting connections */
	ENGINE_API virtual bool IsServer() const;

	ENGINE_API virtual void CleanPackageMaps();

	ENGINE_API void PreSeamlessTravelGarbageCollect();

	ENGINE_API void PostSeamlessTravelGarbageCollect();

	/**
	 * Get the socket subsytem appropriate for this net driver
	 */
	virtual class ISocketSubsystem* GetSocketSubsystem() PURE_VIRTUAL(UNetDriver::GetSocketSubsystem, return NULL;);

	/**
	 * Associate a world with this net driver. 
	 * Disassociates any previous world first.
	 * 
	 * @param InWorld the world to associate with this netdriver
	 */
	ENGINE_API void SetWorld(class UWorld* InWorld);

	/**
	 * Get the world associated with this net driver
	 */
	class UWorld* GetWorld() const override { return World; }

	/** Called during seamless travel to clear all state that was tied to the previous game world (actor lists, etc) */
	ENGINE_API virtual void ResetGameWorldState();

	/** @return true if the net resource is valid or false if it should not be used */
	virtual bool IsNetResourceValid(void) PURE_VIRTUAL(UNetDriver::IsNetResourceValid, return false;);

	bool NetObjectIsDynamic(const UObject *Object) const;

	/** Draws debug markers in the world based on network state */
	void DrawNetDriverDebug();

	/** 
	 * Finds a FRepChangedPropertyTracker associated with an object.
	 * If not found, creates one.
	*/
	TSharedPtr<FRepChangedPropertyTracker> FindOrCreateRepChangedPropertyTracker(UObject *Obj);

	/** Returns true if the client should destroy immediately any actor that becomes torn-off */
	virtual bool ShouldClientDestroyTearOffActors() const { return false; }

	/** Returns whether or not properties that are replicating using this driver should not call RepNotify functions. */
	virtual bool ShouldSkipRepNotifies() const { return false; }

	/** Returns true if actor channels with InGUID should queue up bunches, even if they wouldn't otherwise be queued. */
	virtual bool ShouldQueueBunchesForActorGUID(FNetworkGUID InGUID) const { return false; }

	/** Returns the existing FNetworkGUID of InActor, if it has one. */
	virtual FNetworkGUID GetGUIDForActor(const AActor* InActor) const { return FNetworkGUID(); }

	/** Returns the actor that corresponds to InGUID, if one can be found. */
	virtual AActor* GetActorForGUID(FNetworkGUID InGUID) const { return nullptr; }

	/** Returns true if RepNotifies should be checked and generated when receiving properties for the given object. */
	virtual bool ShouldReceiveRepNotifiesForObject(UObject* Object) const { return true; }

	/** Returns the object that manages the list of replicated UObjects. */
	ENGINE_API FNetworkObjectList& GetNetworkObjectList() { return *NetworkObjects; }

	/** Returns the object that manages the list of replicated UObjects. */
	ENGINE_API const FNetworkObjectList& GetNetworkObjectList() const { return *NetworkObjects; }

	/** Get the network object matching the given Actor, or null if not found. */
	ENGINE_API const FNetworkObjectInfo* GetNetworkObjectInfo(const AActor* InActor) const;

	/** Get the network object matching the given Actor, or null if not found. */
	ENGINE_API FNetworkObjectInfo* GetNetworkObjectInfo(const AActor* InActor);

	DEPRECATED(4.16, "GetNetworkActor is deprecated.  Use GetNetworkObjectInfo instead.")
	ENGINE_API const FNetworkObjectInfo* GetNetworkActor( const AActor* InActor ) const;

	DEPRECATED(4.16, "GetNetworkActor is deprecated.  Use GetNetworkObjectInfo instead.")
	ENGINE_API FNetworkObjectInfo* GetNetworkActor( const AActor* InActor );

	/**
	 * Returns whether adaptive net frequency is enabled. If enabled, update frequency is allowed to ramp down to MinNetUpdateFrequency for an actor when no replicated properties have changed.
	 * This is currently controlled by the CVar "net.UseAdaptiveNetUpdateFrequency".
	 */
	ENGINE_API static bool IsAdaptiveNetUpdateFrequencyEnabled();

	/** Returns true if adaptive net update frequency is enabled and the given actor is having its update rate lowered from its standard rate. */
	ENGINE_API bool IsNetworkActorUpdateFrequencyThrottled(const AActor* InActor) const;

	/** Returns true if adaptive net update frequency is enabled and the given actor is having its update rate lowered from its standard rate. */
	ENGINE_API bool IsNetworkActorUpdateFrequencyThrottled(const FNetworkObjectInfo& InNetworkActor) const;

	/** Stop adaptive replication for the given actor if it's currently throttled. It maybe be allowed to throttle again later. */
	ENGINE_API void CancelAdaptiveReplication(FNetworkObjectInfo& InNetworkActor);

	/** Returns the level ID/PIE instance ID for this netdriver to use. */
	ENGINE_API int32 GetDuplicateLevelID() const { return DuplicateLevelID; }

	/** Sets the level ID/PIE instance ID for this netdriver to use. */
	ENGINE_API void SetDuplicateLevelID(const int32 InDuplicateLevelID) { DuplicateLevelID = InDuplicateLevelID; }

protected:

	/** Adds (fully initialized, ready to go) client connection to the ClientConnections list + any other game related setup */
	ENGINE_API void	AddClientConnection(UNetConnection * NewConnection);

	/** Register all TickDispatch, TickFlush, PostTickFlush to tick in World */
	ENGINE_API void RegisterTickEvents(class UWorld* InWorld);
	/** Unregister all TickDispatch, TickFlush, PostTickFlush to tick in World */
	ENGINE_API void UnregisterTickEvents(class UWorld* InWorld);
	/** Returns true if this actor is considered to be in a loaded level */
	ENGINE_API virtual bool IsLevelInitializedForActor( const AActor* InActor, const UNetConnection* InConnection ) const;

#if WITH_SERVER_CODE
	/**
	* Helper functions for ServerReplicateActors
	*/
	int32 ServerReplicateActors_PrepConnections( const float DeltaSeconds );
	void ServerReplicateActors_BuildConsiderList( TArray<FNetworkObjectInfo*>& OutConsiderList, const float ServerTickTime );
	int32 ServerReplicateActors_PrioritizeActors( UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, const TArray<FNetworkObjectInfo*> ConsiderList, const bool bCPUSaturated, FActorPriority*& OutPriorityList, FActorPriority**& OutPriorityActors );
	int32 ServerReplicateActors_ProcessPrioritizedActors( UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, FActorPriority** PriorityActors, const int32 FinalSortedCount, int32& OutUpdated );
#endif

	/** Used to handle any NetDriver specific cleanup once a level has been removed from the world. */
	ENGINE_API virtual void OnLevelRemovedFromWorld(class ULevel* Level, class UWorld* World);

	/** Handle that tracks OnLevelRemovedFromWorld. */
	FDelegateHandle OnLevelRemovedFromWorldHandle;

private:

	/** Stores the list of objects to replicate into the replay stream. This should be a TUniquePtr, but it appears the generated.cpp file needs the full definition of the pointed-to type. */
	TSharedPtr<FNetworkObjectList> NetworkObjects;

	/** Set to "Lagging" on the server when all client connections are near timing out. We are lagging on the client when the server connection is near timed out. */
	ENetworkLagState::Type LagState;

	/** Duplicate level instance to use for playback (PIE instance ID) */
	int32 DuplicateLevelID;
};
