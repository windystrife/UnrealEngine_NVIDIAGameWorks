// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/CookStats.h"


#if ENABLE_COOK_STATS
CORE_API FCookStatsManager::FGatherCookStatsDelegate FCookStatsManager::CookStatsCallbacks;

CORE_API void FCookStatsManager::LogCookStats(AddStatFuncRef AddStat)
{
	CookStatsCallbacks.Broadcast(AddStat);
}
#endif
