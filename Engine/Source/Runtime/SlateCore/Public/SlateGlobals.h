// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

/* Globals
 *****************************************************************************/

 // Compile all the RichText and MultiLine editable text?
#define WITH_FANCY_TEXT 1

 // If you want to get really verbose stats out of Slate to get a really in-depth
 // view of what widgets are causing you the greatest problems, set this define to 1.
 #define WITH_VERY_VERBOSE_SLATE_STATS 0

// HOW TO GET AN IN-DEPTH PERFORMANCE ANALYSIS OF SLATE
//
// Step 1)
//    Set WITH_VERY_VERBOSE_SLATE_STATS to 1.
//
// Step 2)
//    When running the game (outside of the editor), run these commandline options
//    in order and you'll get a large dump of where all the time is going in Slate.
//    
//    stat group enable slateverbose
//    stat group enable slateveryverbose
//    stat dumpave -root=stat_slate -num=120 -ms=0

SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlate, Log, All);
SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlateStyles, Log, All);

DECLARE_STATS_GROUP(TEXT("Slate Memory"), STATGROUP_SlateMemory, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Slate"), STATGROUP_Slate, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("SlateVerbose"), STATGROUP_SlateVerbose, STATCAT_Advanced);
DECLARE_STATS_GROUP_MAYBE_COMPILED_OUT(TEXT("SlateVeryVerbose"), STATGROUP_SlateVeryVerbose, STATCAT_Advanced, WITH_VERY_VERBOSE_SLATE_STATS);

// Compile slate with a deferred desired size calculation, rather than immediately calculating
// all desired sizes during prepass, only invalidate and wait for it to be requested.
//#define SLATE_DEFERRED_DESIRED_SIZE 0

/* Forward declarations
*****************************************************************************/
class FActiveTimerHandle;
enum class EActiveTimerReturnType : uint8;
