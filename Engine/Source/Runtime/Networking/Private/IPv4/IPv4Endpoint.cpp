// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Interfaces/IPv4/IPv4Endpoint.h"


/* FIPv4Endpoint static initialization
 *****************************************************************************/

const FIPv4Endpoint FIPv4Endpoint::Any(FIPv4Address(0, 0, 0, 0), 0);
ISocketSubsystem* FIPv4Endpoint::CachedSocketSubsystem = nullptr;


/* FIPv4Endpoint interface
 *****************************************************************************/

FString FIPv4Endpoint::ToString() const
{
	return FString::Printf(TEXT("%s:%i"), *Address.ToString(), Port);
}


/* FIPv4Endpoint static interface
 *****************************************************************************/

void FIPv4Endpoint::Initialize()
{
	CachedSocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
}


bool FIPv4Endpoint::Parse(const FString& EndpointString, FIPv4Endpoint& OutEndpoint)
{
	TArray<FString> Tokens;

	if (EndpointString.ParseIntoArray(Tokens, TEXT(":"), false) == 2)
	{
		if (FIPv4Address::Parse(Tokens[0], OutEndpoint.Address))
		{
			OutEndpoint.Port = FCString::Atoi(*Tokens[1]);

			return true;
		}
	}

	return false;
}
