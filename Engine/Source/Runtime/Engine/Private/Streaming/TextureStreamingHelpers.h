// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureStreamingHelpers.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/MemStack.h"

class UTexture2D;

/**
 * Streaming stats
 */

DECLARE_CYCLE_STAT_EXTERN(TEXT("Game Thread Update Time"),STAT_GameThreadUpdateTime,STATGROUP_Streaming, );


// Streaming Details

DECLARE_CYCLE_STAT_EXTERN(TEXT("AddToWorld Time"),STAT_AddToWorldTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("RemoveFromWorld Time"),STAT_RemoveFromWorldTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateLevelStreaming Time"),STAT_UpdateLevelStreamingTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Volume Streaming Tick"),STAT_VolumeStreamingTickTime,STATGROUP_StreamingDetails, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Streaming Volumes"),STAT_VolumeStreamingChecks,STATGROUP_StreamingDetails, );


DECLARE_LOG_CATEGORY_EXTERN(LogContentStreaming, Log, All);

extern float GLightmapStreamingFactor;
extern float GShadowmapStreamingFactor;
extern bool GNeverStreamOutTextures;

//@DEBUG:
// Set to 1 to log all dynamic component notifications
#define STREAMING_LOG_DYNAMIC		0
// Set to 1 to log when we change a view
#define STREAMING_LOG_VIEWCHANGES	0
// Set to 1 to log when levels are added/removed
#define STREAMING_LOG_LEVELS		0
// Set to 1 to log textures that are canceled by CancelForcedTextures()
#define STREAMING_LOG_CANCELFORCED	0

#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
extern TAutoConsoleVariable<int32> CVarSetTextureStreaming;
#endif

extern TAutoConsoleVariable<float> CVarStreamingBoost;
extern TAutoConsoleVariable<int32> CVarStreamingUseFixedPoolSize;
extern TAutoConsoleVariable<int32> CVarStreamingPoolSize;
extern TAutoConsoleVariable<int32> CVarStreamingCheckBuildStatus;
extern TAutoConsoleVariable<int32> CVarStreamingUseMaterialData;
extern TAutoConsoleVariable<int32> CVarStreamingNumStaticComponentsProcessedPerFrame;
extern TAutoConsoleVariable<int32> CVarStreamingDefragDynamicBounds;

struct FTextureStreamingSettings
{
	FTextureStreamingSettings() { Update(); }
	void Update();

	FORCEINLINE bool operator ==(const FTextureStreamingSettings& Rhs) const { return FMemory::Memcmp(this, &Rhs, sizeof(FTextureStreamingSettings)) == 0; }
	FORCEINLINE bool operator !=(const FTextureStreamingSettings& Rhs) const { return FMemory::Memcmp(this, &Rhs, sizeof(FTextureStreamingSettings)) != 0; }

	float MaxEffectiveScreenSize;
	int32 MaxTempMemoryAllowed;
	int32 DropMips;
	int32 HLODStrategy;
	float HiddenPrimitiveScale;
	int32 GlobalMipBias;
	int32 PoolSize;
	bool bLimitPoolSizeToVRAM;
	bool bUseNewMetrics;
	bool bFullyLoadUsedTextures;
	bool bUseAllMips;
	bool bUsePerTextureBias;
	bool bUseMaterialData;
	int32 MinMipForSplitRequest;

protected:

};

typedef TArray<int32, TMemStackAllocator<> > FStreamingRequests;
typedef TArray<const UTexture2D*, TInlineAllocator<12> > FRemovedTextureArray;

#define NUM_BANDWIDTHSAMPLES 512
#define NUM_LATENCYSAMPLES 512

/** Streaming priority: Linear distance factor from 0 to MAX_STREAMINGDISTANCE. */
#define MAX_STREAMINGDISTANCE	10000.0f
#define MAX_MIPDELTA			5.0f
#define MAX_LASTRENDERTIME		90.0f

class UTexture2D;
class UPrimitiveComponent;
class FTextureBoundsVisibility;
class FDynamicTextureInstanceManager;
template<typename T>
class FAsyncTask;
class FAsyncTextureStreamingTask;


struct FStreamingTexture;
struct FStreamingContext;
struct FStreamingHandlerTextureBase;
struct FTexturePriority;

struct FTextureStreamingStats
{
	FTextureStreamingStats() { Reset(); }

	void Reset() { FMemory::Memzero(this, sizeof(FTextureStreamingStats)); }

	void Apply();

	int64 TexturePool;
	// int64 StreamingPool;
	int64 UsedStreamingPool;

	int64 SafetyPool;
	int64 TemporaryPool;
	int64 StreamingPool;
	int64 NonStreamingMips;

	int64 RequiredPool;
	int64 VisibleMips;
	int64 HiddenMips;
	int64 ForcedMips;
	int64 UnkownRefMips;
	int64 CachedMips;

	int64 WantedMips;
	int64 NewRequests; // How much texture memory is required by new requests.
	int64 PendingRequests; // How much texture memory is waiting to be loaded for previous requests.
	int64 MipIOBandwidth;

	int64 OverBudget;

	double Timestamp;

	
	volatile int32 CallbacksCycles;
	int32 SetupAsyncTaskCycles;
	int32 UpdateStreamingDataCycles;
	int32 StreamTexturesCycles;
};
