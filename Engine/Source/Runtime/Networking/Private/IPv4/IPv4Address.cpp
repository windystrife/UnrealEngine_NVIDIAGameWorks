// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Interfaces/IPv4/IPv4Address.h"


/* FIpAddress4 static initialization
 *****************************************************************************/

const FIPv4Address FIPv4Address::Any(0, 0, 0, 0);
const FIPv4Address FIPv4Address::InternalLoopback(127, 0, 0, 1);
const FIPv4Address FIPv4Address::LanBroadcast(255, 255, 255, 255);


/* FIpAddress4 interface
 *****************************************************************************/

FString FIPv4Address::ToString() const
{
	return FString::Printf(TEXT("%i.%i.%i.%i"), A, B, C, D);
}


/* FIpAddress4 static interface
 *****************************************************************************/

bool FIPv4Address::Parse(const FString& AddressString, FIPv4Address& OutAddress)
{
	TArray<FString> Tokens;

	if (AddressString.ParseIntoArray(Tokens, TEXT("."), false) == 4)
	{
		OutAddress.A = FCString::Atoi(*Tokens[0]);
		OutAddress.B = FCString::Atoi(*Tokens[1]);
		OutAddress.C = FCString::Atoi(*Tokens[2]);
		OutAddress.D = FCString::Atoi(*Tokens[3]);

		return true;
	}

	return false;
}
