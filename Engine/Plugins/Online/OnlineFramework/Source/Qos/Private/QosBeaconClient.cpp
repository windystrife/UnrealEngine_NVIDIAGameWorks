// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QosBeaconClient.h"
#include "QosBeaconHost.h"
#include "OnlineSubsystemUtils.h"

AQosBeaconClient::AQosBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	ConnectionStartTime(0.0),
	QosStartTime(0.0),
	bPendingQosRequest(false)
{
}

void AQosBeaconClient::OnConnected()
{
	// Start a timer and wait for a response
	UE_LOG(LogBeacon, Verbose, TEXT("Qos beacon connection established, sending request."));
	ServerQosRequest(DestSessionId);
	QosStartTime = FPlatformTime::Seconds();
	bPendingQosRequest = true;
}

void AQosBeaconClient::SendQosRequest(const FOnlineSessionSearchResult& DesiredHost)
{
	bool bSuccess = false;
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			FString ConnectInfo;
			if (SessionInt->GetResolvedConnectString(DesiredHost, NAME_BeaconPort, ConnectInfo))
			{
				FURL ConnectURL(NULL, *ConnectInfo, TRAVEL_Absolute);
				if (InitClient(ConnectURL) && DesiredHost.Session.SessionInfo.IsValid())
				{
					ConnectionStartTime = FPlatformTime::Seconds();

					DestSessionId = DesiredHost.Session.SessionInfo->GetSessionId().ToString();
					bPendingQosRequest = false;
					bSuccess = true;
				}
				else
				{
					UE_LOG(LogBeacon, Warning, TEXT("SendQosRequest: Failure to init client beacon with %s."), *ConnectURL.ToString());
				}
			}
		}
	}

	if (!bSuccess)
	{
		OnFailure();
	}
}

bool AQosBeaconClient::ServerQosRequest_Validate(const FString& InSessionId)
{
	return !InSessionId.IsEmpty();
}

void AQosBeaconClient::ServerQosRequest_Implementation(const FString& InSessionId)
{
	AQosBeaconHost* BeaconHost = Cast<AQosBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		BeaconHost->ProcessQosRequest(this, InSessionId);
	}
}

void AQosBeaconClient::ClientQosResponse_Implementation(EQosResponseType Response)
{
	double EndTime = FPlatformTime::Seconds();
	UE_LOG(LogBeacon, Display, TEXT("ClientQosResponse: total time: %f RPC time: %f."), EndTime - ConnectionStartTime, EndTime - QosStartTime);

	int32 ResponseTime = FMath::TruncToInt((EndTime - QosStartTime) * 1000.0);
	QosRequestComplete.ExecuteIfBound(EQosResponseType::Success, ResponseTime);
}

