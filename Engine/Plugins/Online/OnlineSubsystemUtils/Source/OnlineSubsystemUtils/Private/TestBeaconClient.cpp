// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TestBeaconClient.h"

ATestBeaconClient::ATestBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}

void ATestBeaconClient::OnFailure()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBeacon, Verbose, TEXT("Test beacon connection failure, handling connection timeout."));
#endif
	Super::OnFailure();
}

/// @cond DOXYGEN_WARNINGS

void ATestBeaconClient::ClientPing_Implementation()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBeacon, Log, TEXT("Ping"));
	ServerPong();
#endif
}

bool ATestBeaconClient::ServerPong_Validate()
{
#if !UE_BUILD_SHIPPING
	return true;
#else
	return false;
#endif
}

void ATestBeaconClient::ServerPong_Implementation()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBeacon, Log, TEXT("Pong"));
	ClientPing();
#endif
}

/// @endcond