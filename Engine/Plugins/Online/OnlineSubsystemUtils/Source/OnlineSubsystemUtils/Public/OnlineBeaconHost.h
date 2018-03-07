// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "OnlineBeacon.h"
#include "OnlineBeaconHost.generated.h"

class AOnlineBeaconClient;
class AOnlineBeaconHostObject;
class FInBunch;
class UNetConnection;
struct FEncryptionKeyResponse;

/**
 * Main actor that listens for side channel communication from another Unreal Engine application
 *
 * The AOnlineBeaconHost listens for connections to route to a registered AOnlineBeaconHostObject 
 * The AOnlineBeaconHostObject is responsible for spawning the server version of the AOnlineBeaconClient
 * The AOnlineBeaconHost pairs the two client actors, verifies the validity of the exchange, and accepts/continues the connection
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API AOnlineBeaconHost : public AOnlineBeacon
{
	GENERATED_UCLASS_BODY()

	/** Configured listen port for this beacon host */
	UPROPERTY(Config)
	int32 ListenPort;

	//~ Begin AActor Interface
	virtual void OnNetCleanup(UNetConnection* Connection) override;
	//~ End AActor Interface

	//~ Begin OnlineBeacon Interface
	virtual void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString) override;
	//~ End OnlineBeacon Interface

	//~ Begin FNetworkNotify Interface
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override;
	//~ End FNetworkNotify Interface

	/**
	 * Initialize the host beacon on a specified port
	 *	Creates the net driver and begins listening for connections
	 *
	 * @return true if host was setup correctly, false otherwise
	 */
	virtual bool InitHost();

	/**
	 * Get the listen port for this beacon
	 *
	 * @return beacon listen port
	 */
	virtual int32 GetListenPort() { return ListenPort; }

	/**
	 * Register a beacon host and its client actor factory
	 *
	 * @param NewHostObject new 
	 */
	virtual void RegisterHost(AOnlineBeaconHostObject* NewHostObject);

	/**
	 * Unregister a beacon host, making future connections of a given type unresponsive
	 *
	 * @param BeaconType type of beacon host to unregister
	 */
	virtual void UnregisterHost(const FString& BeaconType);

	/**
	 * Get the host responsible for a given beacon type
	 *
	 * @param BeaconType type of beacon host 
	 * 
	 * @return BeaconHost for the given type or NULL if that type is not registered
	 */
	AOnlineBeaconHostObject* GetHost(const FString& BeaconType);

	/**
	 * Disconnect a given client from the host
	 *
	 * @param ClientActor the beacon client to disconnect
	 */
	void DisconnectClient(AOnlineBeaconClient* ClientActor);

	/**
	 * Get a client beacon actor for a given connection
	 *
	 * @param Connection connection of interest
	 *
	 * @return client beacon actor that owns the connection
	 */
	virtual AOnlineBeaconClient* GetClientActor(UNetConnection* Connection);

	/**
	 * Remove a client beacon actor from the list of active connections
	 *
	 * @param ClientActor client beacon actor to remove
	 */
	virtual void RemoveClientActor(AOnlineBeaconClient* ClientActor);

private:

	/** List of all client beacon actors with active connections */
	UPROPERTY()
	TArray<AOnlineBeaconClient*> ClientActors;

	/** Sends the welcome control message to the client. Used as a delegate if encryption is being negotiated. */
	void SendWelcomeControlMessage(UNetConnection* Connection);
	void SendWelcomeControlMessage(const FEncryptionKeyResponse& Response, TWeakObjectPtr<UNetConnection> WeakConnection);

	/** Delegate to route a connection attempt to the appropriate beacon host, by type */
	DECLARE_DELEGATE_RetVal_OneParam(AOnlineBeaconClient*, FOnBeaconSpawned, UNetConnection*);
	FOnBeaconSpawned& OnBeaconSpawned(const FString& BeaconType);

	/** Mapping of beacon types to the OnBeaconSpawned delegates */
	TMap<FString, FOnBeaconSpawned> OnBeaconSpawnedMapping;

	/** Delegate to route a connection event to the appropriate beacon host, by type */
	DECLARE_DELEGATE_TwoParams(FOnBeaconConnected, AOnlineBeaconClient*, UNetConnection*);
	FOnBeaconConnected& OnBeaconConnected(const FString& BeaconType);

	/** Mapping of beacon types to the OnBeaconConnected delegates */
	TMap<FString, FOnBeaconConnected> OnBeaconConnectedMapping;
};
