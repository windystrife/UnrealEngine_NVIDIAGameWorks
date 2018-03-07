// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISteamControllerPlugin.h"

#if WITH_STEAM_CONTROLLER == 1


// Disable crazy warnings that claim that standard C library is "deprecated".
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#pragma push_macro("ARRAY_COUNT")
#undef ARRAY_COUNT

#if STEAMSDK_FOUND == 0
#error Steam SDK not located.  Expected to be found in Engine/Source/ThirdParty/Steamworks/{SteamVersion}
#endif

#if USING_CODE_ANALYSIS
 	MSVC_PRAGMA( warning( push ) )
 	MSVC_PRAGMA( warning( disable : ALL_CODE_ANALYSIS_WARNINGS ) )
#endif	// USING_CODE_ANALYSIS

#include "steam/steam_api.h"

#if USING_CODE_ANALYSIS
 	MSVC_PRAGMA( warning( pop ) )
#endif	// USING_CODE_ANALYSIS

#pragma pop_macro("ARRAY_COUNT")

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // WITH_STEAM_CONTROLLER

