// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystem.h"


#define INVALID_INDEX -1

/** Root location of Steam SDK */
#define STEAM_SDK_ROOT_PATH TEXT("Binaries/ThirdParty/Steamworks")

/** URL Prefix when using Steam socket connection */
#define STEAM_URL_PREFIX TEXT("steam.")
/** Filename containing the appid during development */
#define STEAMAPPIDFILENAME TEXT("steam_appid.txt")

/** pre-pended to all steam logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("STEAM: ")

#pragma push_macro("ARRAY_COUNT")
#undef ARRAY_COUNT

// Steamworks SDK headers
#if STEAMSDK_FOUND == 0
#error Steam SDK not located.  Expected to be found in Engine/Source/ThirdParty/Steamworks/{SteamVersion}
#endif

THIRD_PARTY_INCLUDES_START

#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"

THIRD_PARTY_INCLUDES_END

#pragma pop_macro("ARRAY_COUNT")
