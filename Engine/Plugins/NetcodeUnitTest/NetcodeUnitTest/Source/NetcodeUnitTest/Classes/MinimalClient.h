// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/NetConnection.h"
#include "Engine/PendingNetGame.h"

#include "Net/NUTUtilNet.h"
#include "NUTEnum.h"

#include "MinimalClient.generated.h"


// Delegates

/**
 * Delegate for marking the minimal client as having connected fully
 */
DECLARE_DELEGATE(FOnMinClientConnected);

/**
 * Delegate for passing back a network connection failure
 *
 * @param FailureType	The reason for the net failure
 * @param ErrorString	More detailed error information
*/
DECLARE_DELEGATE_TwoParams(FOnMinClientNetworkFailure, ENetworkFailure::Type /*FailureType*/, const FString& /*ErrorString*/);

/**
 * Delegate for hooking the control channel's 'ReceivedBunch' call
 *
 * @param Bunch		The received bunch
*/
DECLARE_DELEGATE_OneParam(FOnMinClientReceivedControlBunch, FInBunch&/* Bunch*/);


/**
 * Delegate for hooking the net connections 'ReceivedRawPacket'
 * 
 * @param Data		The data received
 * @param Count		The number of bytes received
*/
DECLARE_DELEGATE_TwoParams(FOnMinClientReceivedRawPacket, void* /*Data*/, int32& /*Count*/);

/**
 * Delegate for notifying on (and optionally blocking) replicated actor creation
 *
 * @param ActorClass	The class of the actor being replicated
 * @param bActorChannel	Whether or not this actor creation is from an actor channel
 * @param bBlockActor	Whether or not to block creation of the actor (defaults to true)
*/
DECLARE_DELEGATE_ThreeParams(FOnMinClientRepActorSpawn, UClass* /*ActorClass*/, bool /*bActorChannel*/, bool& /*bBlockActor*/);

/**
 * Delegate for notifying AFTER an actor channel actor has been created
 *
 * @param ActorChannel	The actor channel associated with the actor
 * @param Actor			The actor that has just been created
*/
DECLARE_DELEGATE_TwoParams(FOnMinClientNetActor, UActorChannel* /*ActorChannel*/, AActor* /*Actor*/);

/**
 * Delegate for hooking the HandlerClientPlayer event
 *
 * @param PC			The PlayerController being initialized with the net connection
 * @param Connection	The net connection the player is being initialized with
 */
DECLARE_DELEGATE_TwoParams(FOnHandleClientPlayer, APlayerController* /*PC*/, UNetConnection* /*Connection*/);


/**
 * Parameters for configuring the minimal client - also directly inherited by UMinimalClient
 */
struct FMinClientParms
{
	friend class UMinimalClient;


	/** The flags used for configuring the minimal client */
	EMinClientFlags MinClientFlags;

	/** The unit test which owns this minimal client */
	// @todo #JohnB: Eventually remove this interdependency
	UClientUnitTest* Owner;

	/** The address of the launched server */
	FString ServerAddress;

	/** The address of the server beacon (if MinClientFlags are set to connect to a beacon) */
	FString BeaconAddress;

	/** If connecting to a beacon, the beacon type name we are connecting to */
	FString BeaconType;

	/** If overriding the UID used for joining, this specifies it */
	FString JoinUID;

	/** Clientside RPC's that should be allowed to execute (requires the NotifyProcessNetEvent flag) */
	TArray<FString> AllowedClientRPCs;


	/**
	 * Default constructor
	 */
	FMinClientParms()
		: MinClientFlags(EMinClientFlags::None)
		, Owner(nullptr)
		, ServerAddress()
		, BeaconAddress()
		, BeaconType()
		, JoinUID(TEXT("Dud"))
	{
	}

protected:
	/**
	 * Verify that the parameters specified to this struct are valid
	 */
	void ValidateParms();

	void CopyParms(FMinClientParms& Target)
	{
		Target.MinClientFlags = MinClientFlags;
		Target.Owner = Owner;
		Target.ServerAddress = ServerAddress;
		Target.BeaconAddress = BeaconAddress;
		Target.BeaconType = BeaconType;
		Target.JoinUID = JoinUID;
		Target.AllowedClientRPCs = AllowedClientRPCs;
	}
};


/**
 * Delegate hooks for the minimal client - also directly inherited by UMinimalClient
 */
struct FMinClientHooks
{
	friend class UMinimalClient;


	/** Delegate for notifying of successful minimal client connection*/
	FOnMinClientConnected ConnectedDel;

	/** Delegate notifying of network failure */
	FOnMinClientNetworkFailure NetworkFailureDel;

	/** Delegate for notifying/controlling RPC receives */
	FOnProcessNetEvent ReceiveRPCDel;

#if !UE_BUILD_SHIPPING
	/** Delegate for notifying/controlling RPC sends */
	FOnSendRPC SendRPCDel;
#endif

	/** Delegate for notifying of control channel bunches */
	FOnMinClientReceivedControlBunch ReceivedControlBunchDel;

	/** Delegate for notifying of net connection raw packet receives */
	FOnMinClientReceivedRawPacket ReceivedRawPacketDel;

#if !UE_BUILD_SHIPPING
	/** Delegate for notifying of net connection low level packet sends */
	FOnLowLevelSend LowLevelSendDel;
#endif

	/** Delegate for notifying/controlling replicated actor spawning */
	FOnMinClientRepActorSpawn RepActorSpawnDel;

	/** Delegate for notifying AFTER net actor creation */
	FOnMinClientNetActor NetActorDel;

	/** Delegate for notifying of the net connection HandlerClientPlayer event */
	FOnHandleClientPlayer HandleClientPlayerDel;


	/**
	 * Default constructor
	 */
	FMinClientHooks()
		: ConnectedDel()
		, NetworkFailureDel()
		, ReceiveRPCDel()
#if !UE_BUILD_SHIPPING
		, SendRPCDel()
#endif
		, ReceivedControlBunchDel()
		, ReceivedRawPacketDel()
#if !UE_BUILD_SHIPPING
		, LowLevelSendDel()
#endif
		, RepActorSpawnDel()
		, NetActorDel()
		, HandleClientPlayerDel()
	{
	}

protected:
	void CopyHooks(FMinClientHooks& Target)
	{
		Target.ConnectedDel = ConnectedDel;
		Target.NetworkFailureDel = NetworkFailureDel;
		Target.ReceiveRPCDel = ReceiveRPCDel;
#if !UE_BUILD_SHIPPING
		Target.SendRPCDel = SendRPCDel;
#endif
		Target.ReceivedControlBunchDel = ReceivedControlBunchDel;
		Target.ReceivedRawPacketDel = ReceivedRawPacketDel;
#if !UE_BUILD_SHIPPING
		Target.LowLevelSendDel = LowLevelSendDel;
#endif
		Target.RepActorSpawnDel = RepActorSpawnDel;
		Target.NetActorDel = NetActorDel;
		Target.HandleClientPlayerDel = HandleClientPlayerDel;
	}
};

// Sidestep header parser enforcement of 'public' on inheritance below
struct FProtMinClientParms : protected FMinClientParms {};
struct FProtMinClientHooks : protected FMinClientHooks {};


/**
 * Base class for implementing a barebones/stripped-down game client, capable of connecting to a regular game server,
 * but stripped/locked-down so that the absolute minimum of client/server netcode functionality is executed, for connecting the client.
 */
UCLASS()
class NETCODEUNITTEST_API UMinimalClient : public UObject, public FNetworkNotify, public FProtMinClientParms, public FProtMinClientHooks
{
	friend class UUnitTestNetConnection;
	friend class UUnitTestChannel;
	friend class UUnitTestActorChannel;
	friend class UUnitTestPackageMap;

	GENERATED_UCLASS_BODY()

public:
	/**
	 * Connects the minimal client to a server, with Parms specifying the server details and minimal client configuration,
	 * and passing back the low level netcode events specified by Hooks.
	 *
	 * @param Parms		The server parameters and minimal client configuration.
	 * @param Hooks		The delegates for hooking low level netcode events in the minimal client.
	 * @return			Whether or not the connection kicked off successfully.
	 */
	bool Connect(FMinClientParms Parms, FMinClientHooks Hooks);

	/**
	 * Disconnects and cleans up the minimal client.
	 */
	void Cleanup();


	/**
	 * Creates a bunch for the specified channel, with the ability to create the channel as well
	 * WARNING: Can return nullptr! (e.g. if the control channel is saturated)
	 *
	 * @param ChType			Specify the type of channel the bunch will be sent on
	 * @param ChIndex			Specify the index of the channel to send the bunch on (otherwise, picks the next free/unused channel)
	 * @return					Returns the created bunch, or nullptr if it couldn't be created
	 */
	FOutBunch* CreateChannelBunch(EChannelType ChType, int32 ChIndex=INDEX_NONE);

	/**
	 * Sends a bunch over the control channel
	 *
	 * @param ControlChanBunch	The bunch to send over the control channel
	 * @return					Whether or not the bunch was sent successfully
	 */
	bool SendControlBunch(FOutBunch* ControlChanBunch);


	// Accessors

	/**
	 * Retrieves the unit test owning the minimal client
	 */
	FORCEINLINE UClientUnitTest* GetOwner()
	{
		return Owner;
	}

	/**
	 * Retrieve the value of MinClientFlags
	 */
	FORCEINLINE EMinClientFlags GetMinClientFlags()
	{
		return MinClientFlags;
	}

	/**
	 * Retrieve the value of UnitWorld
	 */
	FORCEINLINE UWorld* GetUnitWorld()
	{
		return UnitWorld;
	}

	/**
	 * Retrieve the value of UnitConn
	 */
	FORCEINLINE UNetConnection* GetConn()
	{
		return UnitConn;
	}

	/**
	 * Whether or not the minimal client is connected to the server
	 */
	FORCEINLINE bool IsConnected()
	{
		return bConnected;
	}


	/**
	 * Matches up an active NetConnection to a MinimalClient, if run through a unit test
	 */
	static UMinimalClient* GetMinClientFromConn(UNetConnection* InConn);


	/**
	 * Ticks the minimal client, through the owner unit tests UnitTick function
	 */
	void UnitTick(float DeltaTime);

	/**
	 * Whether or not the minimal client requires ticking.
	 */
	bool IsTickable() const;


protected:
	/**
	 * Creates the minimal client state, and connects to the server
	 *
	 * @return	Whether or not the fake player was successfully created
	 */
	bool ConnectMinimalClient();

	/**
	 * Creates a net driver for the minimal client
	 */
	void CreateNetDriver();

	/**
	 * Sends the packet for triggering the initial join (usually is delayed, by the PacketHandler)
	 */
	void SendInitialJoin();

	/**
	 * Disconnects the minimal client - including destructing the net driver and such (tears down the whole connection)
	 * NOTE: Based upon the HandleDisconnect function, except removing parts that are undesired (e.g. which trigger level switch)
	 */
	void DisconnectMinimalClient();

	/**
	 * See FOnLowLevelSend
	 */
	void NotifySocketSend(void* Data, int32 Count, bool& bBlockSend);

	/**
	 * See FOnReceivedRawPacket
	 */
	void NotifyReceivedRawPacket(void* Data, int32 Count, bool& bBlockReceive);

	/**
	 * See FOnProcessNetEvent
	 */
	void NotifyReceiveRPC(AActor* Actor, UFunction* Function, void* Parameters, bool& bBlockRPC);


	/**
	 * See FOnSendRPC
	 */
	void NotifySendRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject,
						bool& bBlockSendRPC);


protected:
	void InternalNotifyNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type FailureType,
										const FString& ErrorString);



	// FNetworkNotify
protected:
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override
	{
		return EAcceptConnection::Ignore;
	}

	virtual void NotifyAcceptedConnection(UNetConnection* Connection) override
	{
	}

	virtual bool NotifyAcceptingChannel(UChannel* Channel) override;

	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override
	{
	}


	/** Runtime variables */
protected:
	/** Whether or not the minimal client is connected */
	bool bConnected;

	/** Stores a reference to the created fake world, for execution and later cleanup */
	UWorld* UnitWorld;

	/** Stores a reference to the created unit test net driver, for execution and later cleanup */
	UNetDriver* UnitNetDriver;

	/** Stores a reference to the server connection (always a 'UUnitTestNetConnection') */
	UNetConnection* UnitConn;

	/** If notifying of net actor creation, this keeps track of new actor channel indexes pending notification */
	TArray<int32> PendingNetActorChans;


#if TARGET_UE4_CL >= CL_DEPRECATEDEL
private:
	/** Handle to the registered InternalNotifyNetworkFailure delegate */
	FDelegateHandle InternalNotifyNetworkFailureDelegateHandle;
#endif
};
