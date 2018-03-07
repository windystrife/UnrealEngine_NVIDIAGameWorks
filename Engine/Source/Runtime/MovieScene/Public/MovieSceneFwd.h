// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "MovieSceneSequenceID.h"

UENUM()
namespace EMovieScenePlayerStatus
{
	enum Type
	{
		Stopped,
		Playing,
		Recording,
		Scrubbing,
		Jumping,
		Stepping,
		Paused,
		MAX
	};
}

MOVIESCENE_API DECLARE_LOG_CATEGORY_EXTERN(LogMovieScene, Log, All);
DECLARE_STATS_GROUP(TEXT("Movie Scene Evaluation"), STATGROUP_MovieSceneEval, STATCAT_Advanced);

#ifndef MOVIESCENE_DETAILED_STATS
	#define MOVIESCENE_DETAILED_STATS 0
#endif

#if MOVIESCENE_DETAILED_STATS
	#define MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER SCOPE_CYCLE_COUNTER
#else
	#define MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(...)
#endif
