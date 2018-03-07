// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "OnlineBeaconHostObject.h"
#include "QosBeaconHost.generated.h"

class AQosBeaconClient;

/**
 * A beacon host listening for Qos requests from a potential client
 */
UCLASS(transient, config=Engine)
class QOS_API AQosBeaconHost : public AOnlineBeaconHostObject
{
	GENERATED_UCLASS_BODY()

	// Begin AOnlineBeaconHostObject Interface 
	// End AOnlineBeaconHostObject Interface 

	bool Init(FName InSessionName);

	/**
	 * Handle a single Qos request received from an incoming client
	 *
	 * @param Client client beacon making the request
	 * @param SessionId id of the session that is being checked
	 */
	void ProcessQosRequest(AQosBeaconClient* Client, const FString& SessionId);
	bool DoesSessionMatch(const FString& SessionId) const;

	/**
	 * Output current state of beacon to log
	 */
	void DumpState() const;

protected:

	/** Name of session this beacon is associated with */
	FName SessionName;

	/** Running total of Qos requests received since the beacon was created */
	int32 NumQosRequests;
};
