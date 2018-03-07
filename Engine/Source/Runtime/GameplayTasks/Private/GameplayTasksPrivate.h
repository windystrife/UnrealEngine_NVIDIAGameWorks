// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("GameplayTasks"), STATGROUP_GameplayTasks, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("TickGameplayTasks"), STAT_TickGameplayTasks, STATGROUP_GameplayTasks, );
