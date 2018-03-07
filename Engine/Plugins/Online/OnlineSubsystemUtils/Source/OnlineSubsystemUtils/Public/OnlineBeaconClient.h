// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "OnlineBeacon.h"
#include "OnlineBeaconClient.generated.h"

class AOnlineBeaconHostObject;
class FInBunch;
class UNetConnection;
struct FUniqueNetIdRepl;

/**
 * State of a connection.
 */
UENUM()
enum class EBeaconConnectionState : uint8
{
	Invalid = 0,// Connection is invalid, possibly uninitialized.
	Closed = 1,	// Connection permanently closed.
	Pending = 2,// Connection is awaiting connection.
	Open = 3,	// Connection is open.
};

/**
 * Delegate triggered on failures to connect to a host beacon
 */
DECLARE_DELEGATE(FOnHostConnectionFailure);

/**
 * Base class for any unique beacon connectivity, paired with an AOnlineBeaconHostObject implementation 
 *
 * This is the actual actor that replicates across client/server and where all RPCs occur
 * On the host, the life cycle is managed by an AOnlineBeaconHostObject
 * On the client, the life cycle is managed by the game 
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API AOnlineBeaconClient : public AOnlineBeacon
{
	GENERATED_UCLASS_BODY()

	//~ Begin AActor Interface
	virtual bool UseShortConnectTimeout() const override;
	virtual void OnNetCleanup(UNetConnection* Connection) override;
	virtual const AActor* GetNetOwner() const override;
	virtual UNetConnection* GetNetConnection() const override;
	virtual bool DestroyNetworkActorHandled() override;
	//~ End AActor Interface

	//~ Begin FNetworkNotify Interface
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override;
	//~ End FNetworkNotify Interface

	//~ Begin OnlineBeacon Interface
	virtual void OnFailure() override;
	virtual void DestroyBeacon() override;
	//~ End OnlineBeacon Interface

	/**
	 * Initialize the client beacon with connection endpoint
	 *	Creates the net driver and attempts to connect with the destination
	 *
	 * @param URL destination
	 *
	 * @return true if connection is being attempted, false otherwise
	 */
	bool InitClient(FURL& URL);

	/**
	 * Sets the encryption token that will be sent to servers on connection requests as a parameter to the NMT_Hello message.
	 *
	 * @param EncryptionToken the token to use
	 */
	void SetEncryptionToken(const FString& InEncryptionToken);

	/**
	 * Send the packet for triggering the initial join
	 */
	void SendInitialJoin();

	/**
	 * Each beacon must have a unique type identifier
	 *
	 * @return string representing the type of beacon 
	 */
	FString GetBeaconType() const;
	
	/**
	 * A connection has been made and RPC/replication can begin
	 */
	virtual void OnConnected() {};

	/**
	 * Delegate triggered on failures to connect to a host beacon
	 */
	FOnHostConnectionFailure& OnHostConnectionFailure() { return HostConnectionFailure; }

	/**
	 * Set the connection state
	 * Higher level than the net connection because of the handshaking of the actors
	 *
	 * @return connection state of the beacon
	 */
	void SetConnectionState(EBeaconConnectionState NewConnectionState);

	/**
	 * Get the unique id of the user on this connection (server side only)
	 *
	 * @return unique id of the user on this connection
	 */
	const FUniqueNetIdRepl& GetUniqueId() const;

	/**
	 * Get the connection state
	 * Higher level than the net connection because of the handshaking of the actors
	 *
	 * @return connection state of the beacon
	 */
	EBeaconConnectionState GetConnectionState() const;

	/**
	 * Get the owner of this beacon actor, some host that is listening for connections
	 * (server side only, clients have no access)
	 *
	 * @return owning host of this actor
	 */
	AOnlineBeaconHostObject* GetBeaconOwner() const;
	
	/**
	 * Set the owner of this beacon actor, some host that is listening for connections
	 * (server side only, clients have no access)
	 *
	 * @return owning host of this actor
	 */
	void SetBeaconOwner(AOnlineBeaconHostObject* InBeaconOwner);

	/**
	 * Associate this beacon with a network connection
	 *
	 * @param NetConnection connection that the beacon will communicate over
	 */
	virtual void SetNetConnection(UNetConnection* NetConnection)
	{
		BeaconConnection = NetConnection;
	}

protected:

	/** Owning beacon host of this beacon actor (server only) */
	UPROPERTY()
	AOnlineBeaconHostObject* BeaconOwner;

	/** Network connection associated with this beacon client instance */
	UPROPERTY()
	UNetConnection* BeaconConnection;

	/** State of the connection */
	UPROPERTY()
	EBeaconConnectionState ConnectionState;

	/** Delegate for host beacon connection failures */
	FOnHostConnectionFailure HostConnectionFailure;

	/** Handle for efficient management of OnFailure timer */
	FTimerHandle TimerHandle_OnFailure;

private:

	/** Token sent to servers when connecting with an NMT_Hello message. */
	FString EncryptionToken;

	/**
	 * Setup the connection for encryption with a given key
	 * All future packets are expected to be encrypted
	 *
	 * @param Response response from the game containing its encryption key or an error message
	 * @param WeakConnection the connection related to the encryption request
	 */
	void FinalizeEncryptedConnection(const FEncryptionKeyResponse& Response, TWeakObjectPtr<UNetConnection> WeakConnection);

	/**
	 * Called on the server side to open up the actor channel that will allow RPCs to occur
	 * (DO NOT OVERLOAD, implement OnConnected() instead)
	 */
	UFUNCTION(client, reliable)
	void ClientOnConnected();

	friend class AOnlineBeaconHost;
	friend class AOnlineBeaconHostObject;
};

inline const TCHAR* ToString(EBeaconConnectionState Value)
{
	switch (Value)
	{
	case EBeaconConnectionState::Invalid:
	{
		return TEXT("Invalid");
	}
	case EBeaconConnectionState::Closed:
	{
		return TEXT("Closed");
	}
	case EBeaconConnectionState::Pending:
	{
		return TEXT("Pending");
	}
	case EBeaconConnectionState::Open:
	{
		return TEXT("Open");
	}
	}
	return TEXT("");
}
