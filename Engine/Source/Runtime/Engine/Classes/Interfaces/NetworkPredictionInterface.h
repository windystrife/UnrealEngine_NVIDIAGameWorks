// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// INetworkPredictionInterface is an interface for objects that want to perform
// network prediction of movement. See UCharacterMovementComponent for an example implementation.
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "NetworkPredictionInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNetworkPredictionInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ENGINE_API INetworkPredictionInterface
{
	GENERATED_IINTERFACE_BODY()

	//--------------------------------
	// Server hooks
	//--------------------------------

	/** (Server) Send position to client if necessary, or just ack good moves. */
	virtual void SendClientAdjustment()					PURE_VIRTUAL(INetworkPredictionInterface::SendClientAdjustment,);

	/** (Server) Trigger a position update on clients, if the server hasn't heard from them in a while. */
	virtual void ForcePositionUpdate(float DeltaTime)	PURE_VIRTUAL(INetworkPredictionInterface::ForcePositionUpdate,);

	//--------------------------------
	// Client hooks
	//--------------------------------

	/** (Client) After receiving a network update of position, allow some custom smoothing, given the old transform before the correction and new transform from the update. */
	virtual void SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation) PURE_VIRTUAL(INetworkPredictionInterface::SmoothCorrection,);

	//--------------------------------
	// Other
	//--------------------------------

	/** @return FNetworkPredictionData_Client instance used for network prediction. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const PURE_VIRTUAL(INetworkPredictionInterface::GetPredictionData_Client, return NULL;);
	
	/** @return FNetworkPredictionData_Server instance used for network prediction. */
	virtual class FNetworkPredictionData_Server* GetPredictionData_Server() const PURE_VIRTUAL(INetworkPredictionInterface::GetPredictionData_Server, return NULL;);

	/** Accessor to check if there is already client data, without potentially allocating it on demand.*/
	virtual bool HasPredictionData_Client() const PURE_VIRTUAL(INetworkPredictionInterface::HasPredictionData_Client, return false;);

	/** Accessor to check if there is already server data, without potentially allocating it on demand.*/
	virtual bool HasPredictionData_Server() const PURE_VIRTUAL(INetworkPredictionInterface::HasPredictionData_Server, return false;);

	/** Resets client prediction data. */
	virtual void ResetPredictionData_Client() PURE_VIRTUAL(INetworkPredictionInterface::ResetPredictionData_Client,);

	/** Resets server prediction data. */
	virtual void ResetPredictionData_Server() PURE_VIRTUAL(INetworkPredictionInterface::ResetPredictionData_Server,);
};


// Network data representation on the client
class ENGINE_API FNetworkPredictionData_Client
{
public:

	FNetworkPredictionData_Client()
	{
	}

	virtual ~FNetworkPredictionData_Client() {}
};


// Network data representation on the server
class ENGINE_API FNetworkPredictionData_Server
{
public:

	FNetworkPredictionData_Server()
	: ServerTimeStamp(0.f)
	{
	}

	virtual ~FNetworkPredictionData_Server() {}

	/** Server clock time when last server move was received or movement was forced to be processed */
	float ServerTimeStamp;
};

