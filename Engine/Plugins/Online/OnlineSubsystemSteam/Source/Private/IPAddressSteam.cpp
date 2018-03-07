// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IPAddressSteam.h"

/**
 * Sets the ip address from a string ("steam.STEAMID" or "STEAMID")
 *
 * @param InAddr the string containing the new ip address to use
 * @param bIsValid was the address valid for use
 */
void FInternetAddrSteam::SetIp(const TCHAR* InAddr, bool& bIsValid)
{
	bIsValid = false;
	FString InAddrStr(InAddr);

	// Check for the steam. prefix
	FString SteamIPAddrStr;
	if (InAddrStr.StartsWith(STEAM_URL_PREFIX))
	{
		SteamIPAddrStr = InAddrStr.Mid(ARRAY_COUNT(STEAM_URL_PREFIX) - 1);
	}
	else
	{
		SteamIPAddrStr = InAddrStr;
	}

	// Resolve the address/port
	FString SteamIPStr, SteamChannelStr;
	if (SteamIPAddrStr.Split(":", &SteamIPStr, &SteamChannelStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		const uint64 Id = FCString::Atoi64(*SteamIPStr);
		if (Id != 0)
		{
			SteamId = FUniqueNetIdSteam(Id);
			const int32 Channel = FCString::Atoi(*SteamChannelStr);
			if (Channel != 0 || SteamChannelStr == "0")
			{
				SteamChannel = Channel;
				bIsValid = true;
			}
		}
	}
	else
	{
		const uint64 Id = FCString::Atoi64(*SteamIPAddrStr);
		if (Id != 0)
		{
			SteamId = FUniqueNetIdSteam(Id);
			bIsValid = true;
		}

		SteamChannel = 0;
	}

	bIsValid = bIsValid && SteamId.IsValid();
}
