// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Interfaces/Steam/SteamEndpoint.h"


/* FSteamEndpoint interface
 *****************************************************************************/

FString FSteamEndpoint::ToString() const
{
	return FString::Printf(TEXT("0x%llX:%i"), UniqueNetId, SteamChannel);
}
