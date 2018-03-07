// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QosBeaconHost.h"
#include "Engine/NetConnection.h"
#include "QosBeaconClient.h"
#include "OnlineSubsystemUtils.h"

AQosBeaconHost::AQosBeaconHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	SessionName(NAME_None),
	NumQosRequests(0)
{
	ClientBeaconActorClass = AQosBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}

bool AQosBeaconHost::Init(FName InSessionName)
{
	SessionName = InSessionName;
	NumQosRequests = 0;
	return true;
}

bool AQosBeaconHost::DoesSessionMatch(const FString& SessionId) const
{
	UWorld* World = GetWorld();

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	FNamedOnlineSession* Session = SessionInt.IsValid() ? SessionInt->GetNamedSession(SessionName) : NULL;
	if (Session && Session->SessionInfo.IsValid() && !SessionId.IsEmpty() && Session->SessionInfo->GetSessionId().ToString() == SessionId)
	{
		return true;
	}

	return false;
}

void AQosBeaconHost::ProcessQosRequest(AQosBeaconClient* Client, const FString& SessionId)
{
	UE_LOG(LogBeacon, Verbose, TEXT("ProcessQosRequest %s SessionId %s from (%s)"),
		Client ? *Client->GetName() : TEXT("NULL"),
		*SessionId,
		Client ? *Client->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));

	NumQosRequests++;

	if (Client)
	{
		if (DoesSessionMatch(SessionId))
		{
			Client->ClientQosResponse(EQosResponseType::Success);
		}
		else
		{
			Client->ClientQosResponse(EQosResponseType::Failure);
		}
	}
}

void AQosBeaconHost::DumpState() const
{
	UE_LOG(LogBeacon, Display, TEXT("Qos Beacon:"), *GetBeaconType());
	UE_LOG(LogBeacon, Display, TEXT("Session that beacon is for: %s"), *SessionName.ToString());
	UE_LOG(LogBeacon, Display, TEXT("Number of Qos requests: %d"), NumQosRequests);
}
