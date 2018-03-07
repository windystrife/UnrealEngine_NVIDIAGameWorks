// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"


LANDSCAPE_API DECLARE_LOG_CATEGORY_EXTERN(LogLandscape, Warning, All);

/**
 * Landscape stats
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Landscape Dynamic Draw Time"), STAT_LandscapeDynamicDrawTime, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Landscape Static Draw LOD Time"), STAT_LandscapeStaticDrawLODTime, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LandscapeVF Draw Time"), STAT_LandscapeVFDrawTime, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Landscape Triangles"), STAT_LandscapeTriangles, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Landscape Render Passes"), STAT_LandscapeComponents, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Landscape DrawCalls"), STAT_LandscapeDrawCalls, STATGROUP_Landscape, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Landscape Vertex Mem"), STAT_LandscapeVertexMem, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Component Mem"), STAT_LandscapeComponentMem, STATGROUP_Landscape, );
