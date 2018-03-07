// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Engine/PendingNetGame.h"
#include "Engine/NetDriver.h"
#include "OnlineBeacon.generated.h"

class FInBunch;
class UChannel;
class UNetConnection;

ONLINESUBSYSTEMUTILS_API DECLARE_LOG_CATEGORY_EXTERN(LogBeacon, Display, All);

/** States that a beacon can be in */
namespace EBeaconState
{
	enum Type
	{
		AllowRequests,
		DenyRequests
	};
}

/**
 * Base class for beacon communication (Unreal Networking, but outside normal gameplay traffic)
 */
UCLASS(transient, config=Engine, notplaceable)
class ONLINESUBSYSTEMUTILS_API AOnlineBeacon : public AActor, public FNetworkNotify
{
	GENERATED_UCLASS_BODY()

	//~ Begin AActor Interface
	virtual void OnActorChannelOpen(FInBunch& InBunch, UNetConnection* Connection) override;
	virtual bool IsRelevancyOwnerFor(const AActor* ReplicatedActor, const AActor* ActorOwner, const AActor* ConnectionActor) const override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	virtual const AActor* GetNetOwner() const override { return nullptr; }
	virtual UNetConnection* GetNetConnection() const override { return nullptr; }
	virtual bool IsLevelBoundsRelevant() const override { return false; }
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	//~ End AActor Interface

	//~ Begin FNetworkNotify Interface
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;
	virtual void NotifyAcceptedConnection(UNetConnection* Connection) override;
	virtual bool NotifyAcceptingChannel(UChannel* Channel) override;
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override;
	//~ End FNetworkNotify Interface
	
    /**
	 * Get the current state of the beacon
	 *
	 * @return state of the beacon
	 */
	EBeaconState::Type GetBeaconState() const { return BeaconState; }

	/**
	 * Notification of network error messages, allows a beacon to handle the failure
	 *
	 * @param	World associated with failure
	 * @param	NetDriver associated with failure
	 * @param	FailureType	the type of error
	 * @param	ErrorString	additional string detailing the error
	 */
	virtual void HandleNetworkFailure(UWorld* World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Set Beacon state 
	 *
	 * @bPause should the beacon stop accepting requests
	 */
	void PauseBeaconRequests(bool bPause)
	{
		if (bPause)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("All Beacon Requests Paused."));
			NetDriver->SetWorld(nullptr);
			NetDriver->Notify = this;
			BeaconState = EBeaconState::DenyRequests;
		}
		else
		{
			UE_LOG(LogBeacon, Verbose, TEXT("All Beacon Requests Resumed."));
			NetDriver->SetWorld(GetWorld());
			NetDriver->Notify = this;
			BeaconState = EBeaconState::AllowRequests;
		}
	}

	/** Beacon cleanup and net driver destruction */
	virtual void DestroyBeacon();

protected:

	/** Time beacon will wait to establish a connection with the beacon host */
	UPROPERTY(Config)
	float BeaconConnectionInitialTimeout;
	/** Time beacon will wait for packets after establishing a connection before giving up */
	UPROPERTY(Config)
	float BeaconConnectionTimeout;

	/** Net driver routing network traffic */
	UPROPERTY()
	UNetDriver* NetDriver;

	/** State of beacon */
	EBeaconState::Type BeaconState;
	/** Handle to the registered HandleNetworkFailure delegate */
	FDelegateHandle HandleNetworkFailureDelegateHandle;

	/** Common initialization for all beacon types */
	virtual bool InitBase();

	/** Notification that failure needs to be handled */
	virtual void OnFailure();

	/** overridden to return that player controllers are capable of RPCs */
	virtual bool HasNetOwner() const override;
};
