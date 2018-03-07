// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Interfaces/IPv4/IPv4SubnetMask.h"


/* FIpAddress4 interface
 *****************************************************************************/

FString FIPv4SubnetMask::ToString() const
{
	return FString::Printf(TEXT("%i.%i.%i.%i"), A, B, C, D);
}


/* FIpAddress4 static interface
 *****************************************************************************/

bool FIPv4SubnetMask::Parse(const FString& MaskString, FIPv4SubnetMask& OutMask)
{
	TArray<FString> Tokens;

	if (MaskString.ParseIntoArray(Tokens, TEXT("."), false) == 4)
	{
		OutMask.A = FCString::Atoi(*Tokens[0]);
		OutMask.B = FCString::Atoi(*Tokens[1]);
		OutMask.C = FCString::Atoi(*Tokens[2]);
		OutMask.D = FCString::Atoi(*Tokens[3]);

		return true;
	}

	return false;
}
