// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HAL/LowLevelMemTracker.h"
#include "ScopeLock.h"
#include "LowLevelMemoryUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#define ENABLE_MEMPRO 0		// (note: MemPro.cpp/need to be added to the project for this to work)

// There is a little memory and cpu overhead in tracking peak memory but it is generally more useful than current memory.
// Disable if you need a little more memory or speed
#define LLM_TRACK_PEAK_MEMORY 0

#if ENABLE_MEMPRO
#include "MemPro.hpp"
bool START_MEMPRO = true;
ELLMTag MemProTrackTag = ELLMTag::TaskGraphTasksMisc;		// MaxUserAllocation to track all allocs
#endif

TAutoConsoleVariable<int32> CVarLLMWriteInterval(
	TEXT("LLM.LLMWriteInterval"),
	5,
	TEXT("The number of seconds between each line in the LLM csv (zero to write every frame)")
);

DECLARE_LLM_MEMORY_STAT(TEXT("LLM Overhead"), STAT_LLMOverheadTotal, STATGROUP_LLMOverhead);

DEFINE_STAT(STAT_EngineSummaryLLM);

/*
 * LLM stats referenced by ELLMTagNames
 */
DECLARE_LLM_MEMORY_STAT(TEXT("Total"), STAT_LLMPlatformTotal, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Untracked"), STAT_LLMPlatformTotalUntracked, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Tracked Total"), STAT_TrackedTotalLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Untagged"), STAT_UntrackedTotalLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Tracked Total"), STAT_PlatformTrackedTotalLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Untagged"), STAT_PlatformUntrackedTotalLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("SmallBinnedAllocation"), STAT_SmallBinnedAllocationLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("LargeBinnedAllocation"), STAT_LargeBinnedAllocationLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("ThreadStack"), STAT_ThreadStackLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Program Size"), STAT_ProgramSizePlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Program Size"), STAT_ProgramSizeLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("OOM Backup Pool"), STAT_OOMBackupPoolPlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("OOM Backup Pool"), STAT_OOMBackupPoolLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GenericPlatformMallocCrash"), STAT_GenericPlatformMallocCrashLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GenericPlatformMallocCrash"), STAT_GenericPlatformMallocCrashPlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Engine Misc"), STAT_EngineMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("TaskGraph Tasks (misc)"), STAT_TaskGraphTasksMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Audio"), STAT_AudioLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("FName"), STAT_FNameLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Networking"), STAT_NetworkingLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Meshes"), STAT_MeshesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Stats"), STAT_StatsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Shaders"), STAT_ShadersLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Textures"), STAT_TexturesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Render Targets"), STAT_RenderTargetsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("RHIMisc"), STAT_RHIMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysX (TriMesh)"), STAT_PhysXTriMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysX (ConvexMesh)"), STAT_PhysXConvexMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AsyncLoading"), STAT_AsyncLoadingLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("UObject"), STAT_UObjectLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Animation"), STAT_AnimationLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("StaticMesh"), STAT_StaticMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Materials"), STAT_MaterialsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Particles"), STAT_ParticlesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GC"), STAT_GCLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("UI"), STAT_UILLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysX"), STAT_PhysXLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("EnginePreInit"), STAT_EnginePreInitLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("EngineInit"), STAT_EngineInitLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Rendering Thread"), STAT_RenderingThreadLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("LoadMap Misc"), STAT_LoadMapMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("StreamingManager"), STAT_StreamingManagerLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Graphics"), STAT_GraphicsPlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("FileSystem"), STAT_FileSystemLLM, STATGROUP_LLMFULL);

/*
* LLM Summary stats referenced by ELLMTagNames
*/
DECLARE_LLM_MEMORY_STAT(TEXT("Total"), STAT_TrackedTotalSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("Audio"), STAT_AudioSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("Meshes"), STAT_MeshesSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysX"), STAT_PhysXSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("UObject"), STAT_UObjectSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("Animation"), STAT_AnimationSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("StaticMesh"), STAT_StaticMeshSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("Materials"), STAT_MaterialsSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("Particles"), STAT_ParticlesSummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("UI"), STAT_UISummaryLLM, STATGROUP_LLM);
DECLARE_LLM_MEMORY_STAT(TEXT("Textures"), STAT_TexturesSummaryLLM, STATGROUP_LLM);



struct FLLMTagInfo
{
	const TCHAR* Name;
	FName StatName;				// shows in the LLMFULL stat group
	FName SummaryStatName;		// shows in the LLM stat group
};

// *** order must match ELLMTag enum ***
const FLLMTagInfo ELLMTagNames[] =
{
	// CSV name								LLM Stat stat name									LLM Summary stat name							// enum value
	{ TEXT("Untagged"),						NAME_None,											NAME_None },									// ELLMTag::Untagged
	{ TEXT("Paused"),						NAME_None,											NAME_None },									// ELLMTag::Paused
	{ TEXT("Tracked Total"),				GET_STATFNAME(STAT_TrackedTotalLLM),				GET_STATFNAME(STAT_TrackedTotalSummaryLLM) },	// ELLMTag::TrackedTotal
	{ TEXT("Untagged"),						GET_STATFNAME(STAT_UntrackedTotalLLM),				NAME_None },									// ELLMTag::UntrackedTotal
	{ TEXT("Tracked Total"),				GET_STATFNAME(STAT_PlatformTrackedTotalLLM),		NAME_None },									// ELLMTag::PlatformTrackedTotal
	{ TEXT("Untagged"),						GET_STATFNAME(STAT_PlatformUntrackedTotalLLM),		NAME_None },									// ELLMTag::PlatformUntrackedTotal
	{ TEXT("SmallBinnedAllocation"),		GET_STATFNAME(STAT_SmallBinnedAllocationLLM),		NAME_None },									// ELLMTag::SmallBinnedAllocation
	{ TEXT("LargeBinnedAllocation"),		GET_STATFNAME(STAT_LargeBinnedAllocationLLM),		NAME_None },									// ELLMTag::LargeBinnedAllocation
	{ TEXT("ThreadStack"),					GET_STATFNAME(STAT_ThreadStackLLM),					NAME_None },									// ELLMTag::ThreadStack
	{ TEXT("Program Size"),					GET_STATFNAME(STAT_ProgramSizePlatformLLM),			NAME_None },									// ELLMTag::ProgramSizePlatform
	{ TEXT("Program Size"),					GET_STATFNAME(STAT_ProgramSizeLLM),					GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::ProgramSize
	{ TEXT("OOM Backup Pool"),				GET_STATFNAME(STAT_OOMBackupPoolPlatformLLM),		NAME_None },									// ELLMTag::BackupOOMMemoryPoolPlatform
	{ TEXT("OOM Backup Pool"),				GET_STATFNAME(STAT_OOMBackupPoolLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::BackupOOMMemoryPool
	{ TEXT("GenericPlatformMallocCrash"),	GET_STATFNAME(STAT_GenericPlatformMallocCrashLLM),	GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::GenericPlatformMallocCrash
	{ TEXT("GenericPlatformMallocCrash"),	GET_STATFNAME(STAT_GenericPlatformMallocCrashPlatformLLM),	GET_STATFNAME(STAT_EngineSummaryLLM) },	// ELLMTag::GenericPlatformMallocCrashPlatform
	{ TEXT("Engine Misc"),					GET_STATFNAME(STAT_EngineMiscLLM),					GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::EngineMisc
	{ TEXT("TaskGraph Tasks (misc)"),		GET_STATFNAME(STAT_TaskGraphTasksMiscLLM),			GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::TaskGraphTasksMisc
	{ TEXT("Audio"),						GET_STATFNAME(STAT_AudioLLM),						GET_STATFNAME(STAT_AudioSummaryLLM) },			// ELLMTag::Audio
	{ TEXT("FName"),						GET_STATFNAME(STAT_FNameLLM),						GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::FName
	{ TEXT("Networking"),					GET_STATFNAME(STAT_NetworkingLLM),					GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::Networking
	{ TEXT("Meshes"),						GET_STATFNAME(STAT_MeshesLLM),						GET_STATFNAME(STAT_MeshesSummaryLLM) },			// ELLMTag::Meshes
	{ TEXT("Stats"),						GET_STATFNAME(STAT_StatsLLM),						GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::Stats
	{ TEXT("Shaders"),						GET_STATFNAME(STAT_ShadersLLM),						GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::Shaders
	{ TEXT("Textures"),						GET_STATFNAME(STAT_TexturesLLM),					GET_STATFNAME(STAT_TexturesSummaryLLM) },			// ELLMTag::Textures
	{ TEXT("Render Targets"),				GET_STATFNAME(STAT_RenderTargetsLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::RenderTargets
	{ TEXT("RHI Misc"),						GET_STATFNAME(STAT_RHIMiscLLM),						GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::RHIMisc
	{ TEXT("PhysX (TriMesh)"),				GET_STATFNAME(STAT_PhysXTriMeshLLM),				GET_STATFNAME(STAT_PhysXSummaryLLM) },			// ELLMTag::PhysXTriMesh
	{ TEXT("PhysX (ConvexMesh)"),			GET_STATFNAME(STAT_PhysXConvexMeshLLM),				GET_STATFNAME(STAT_PhysXSummaryLLM) },			// ELLMTag::PhysXConvexMesh
	{ TEXT("AsyncLoading"),					GET_STATFNAME(STAT_AsyncLoadingLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::AsyncLoading
	{ TEXT("UObject"),						GET_STATFNAME(STAT_UObjectLLM),						GET_STATFNAME(STAT_UObjectSummaryLLM) },		// ELLMTag::UObject
	{ TEXT("Animation"),					GET_STATFNAME(STAT_AnimationLLM),					GET_STATFNAME(STAT_AnimationSummaryLLM) },		// ELLMTag::Animation
	{ TEXT("StaticMesh"),					GET_STATFNAME(STAT_StaticMeshLLM),					GET_STATFNAME(STAT_StaticMeshSummaryLLM) },		// ELLMTag::StaticMesh
	{ TEXT("Materials"),					GET_STATFNAME(STAT_MaterialsLLM),					GET_STATFNAME(STAT_MaterialsSummaryLLM) },		// ELLMTag::Materials
	{ TEXT("Particles"),					GET_STATFNAME(STAT_ParticlesLLM),					GET_STATFNAME(STAT_ParticlesSummaryLLM) },		// ELLMTag::Particles
	{ TEXT("GC"),							GET_STATFNAME(STAT_GCLLM),							GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::GC
	{ TEXT("UI"),							GET_STATFNAME(STAT_UILLM),							GET_STATFNAME(STAT_UISummaryLLM) },				// ELLMTag::UI
	{ TEXT("PhysX"),						GET_STATFNAME(STAT_PhysXLLM),						GET_STATFNAME(STAT_PhysXSummaryLLM) },			// ELLMTag::PhysX
	{ TEXT("EnginePreInit"),				GET_STATFNAME(STAT_EnginePreInitLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::EnginePreInitMemory
	{ TEXT("EngineInit"),					GET_STATFNAME(STAT_EngineInitLLM),					GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::EngineInitMemory
	{ TEXT("Rendering Thread"),				GET_STATFNAME(STAT_RenderingThreadLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::RenderingThreadMemory
	{ TEXT("LoadMap Misc"),					GET_STATFNAME(STAT_LoadMapMiscLLM),					GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::LoadMapMisc
	{ TEXT("StreamingManager"),				GET_STATFNAME(STAT_StreamingManagerLLM),			GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::StreamingManager
	{ TEXT("Graphics"),						GET_STATFNAME(STAT_GraphicsPlatformLLM),			NAME_None },									// ELLMTag::GraphicsPlatform
	{ TEXT("FileSystem"),					GET_STATFNAME(STAT_FileSystemLLM),					GET_STATFNAME(STAT_EngineSummaryLLM) },			// ELLMTag::FileSystem
};

static_assert(sizeof(ELLMTagNames) / sizeof(FLLMTagInfo) == (int32)ELLMTag::GenericTagCount, "Please update ELLMTagNames to match the ELLMTag enum");

/**
 * FLLMCsvWriter: class for writing out the LLM stats to a csv file every few seconds
 */
class FLLMCsvWriter
{
public:
	FLLMCsvWriter();

	~FLLMCsvWriter();

	void SetAllocator(FLLMAllocator* Allocator)
	{
		StatValues.SetAllocator(Allocator);
		StatValuesForWrite.SetAllocator(Allocator);
	}

	void SetTracker(ELLMTracker InTracker) { Tracker = InTracker; }

	void Clear();

#if LLM_TRACK_PEAK_MEMORY
	void AddStat(int64 Tag, int64 Value, int64 Peak);
	void SetStat(int64 Tag, int64 Value, int64 Peak);
#else
	void AddStat(int64 Tag, int64 Value);
	void SetStat(int64 Tag, int64 Value);
#endif

	void Update(FLLMPlatformTag* PlatformTags);

	void SetEnabled(bool value) { Enabled = value; }

private:
	void WriteGraph(FLLMPlatformTag* PlatformTags);

	void Write(const FString& Text);

	static FString GetTagName(int64 Tag, FLLMPlatformTag* PlatformTags);

	static const TCHAR* GetTrackerCsvName(ELLMTracker InTracker);

	struct StatValue
	{
		int64 Tag;
		int64 Value;
#if LLM_TRACK_PEAK_MEMORY
		int64 Peak;
#endif
	};

	bool Enabled;

	ELLMTracker Tracker;

	FLLMArray<StatValue> StatValues;
	FLLMArray<StatValue> StatValuesForWrite;

	int32 WriteCount;

	FCriticalSection StatValuesLock;

	double LastWriteTime;

	FArchive* Archive;

	int32 LastWriteStatValueCount;
};

/*
 * this is really the main LLM class. It owns the thread state objects.
 */
class FLLMTracker
{
public:

	FLLMTracker();
	~FLLMTracker();

	void Initialise(ELLMTracker Tracker, FLLMAllocator* InAllocator);

	void PushTag(int64 Tag);
	void PopTag();
#if LLM_ALLOW_ASSETS_TAGS
	void PushAssetTag(int64 Tag);
	void PopAssetTag();
#endif
	void TrackAllocation(const void* Ptr, uint64 Size, ELLMTag DefaultTag, ELLMTracker Tracker);
	void TrackFree(const void* Ptr, uint64 CheckSize, ELLMTracker Tracker);
	void OnAllocMoved(const void* Dest, const void* Source);

	void TrackMemory(int64 Tag, int64 Amount);

	// This will pause/unpause tracking, and also manually increment a given tag
	void PauseAndTrackMemory(int64 Tag, int64 Amount);
	void Pause();
	void Unpause();
	bool IsPaused();
	
	void Clear();

	void SetCSVEnabled(bool Value);

	void WriteCsv(FLLMPlatformTag* PlatformTags);

#define LLM_USE_ALLOC_INFO_STRUCT (LLM_STAT_TAGS_ENABLED || LLM_ALLOW_ASSETS_TAGS)

#if LLM_USE_ALLOC_INFO_STRUCT
	struct FLowLevelAllocInfo
	{
		int64 Tag;
		
		#if LLM_ALLOW_ASSETS_TAGS
			int64 AssetTag;
		#endif
	};
#else
	typedef ELLMTag FLowLevelAllocInfo;
#endif

	typedef LLMMap<PointerKey, uint32, FLowLevelAllocInfo> LLMMap;	// pointer, size, info

	LLMMap& GetAllocationMap()
	{
		return AllocationMap;
	}

	void SetTotalTags(ELLMTag Untagged, ELLMTag Tracked);
	uint64 UpdateFrameAndReturnTotalTrackedMemory(FLLMPlatformTag* PlatformTags);

protected:

	// per thread state, uses system malloc function to be allocated (like FMalloc*)
	class FLLMThreadState
	{
	public:
		FLLMThreadState();

		void SetAllocator(FLLMAllocator* InAllocator);

		void Clear();

		void PushTag(int64 Tag);
		void PopTag();
		int64 GetTopTag();
#if LLM_ALLOW_ASSETS_TAGS
		void PushAssetTag(int64 Tag);
		void PopAssetTag();
		int64 GetTopAssetTag();
#endif
		void TrackAllocation(const void* Ptr, uint64 Size, ELLMTag DefaultTag, ELLMTracker Tracker);
		void TrackFree(const void* Ptr, int64 Tag, uint64 Size, bool bTrackedUntagged, ELLMTracker Tracker);
		void IncrTag(int64 Tag, int64 Amount, bool bTrackUntagged);

		void UpdateFrame(
			ELLMTag InUntaggedTotalTag,
			FLLMThreadState& InLocalState,
			FLLMCsvWriter& CsvWriter,
			FLLMPlatformTag* PlatformTags);

		static void IncMemoryStatByFName(FName Name, int64 Amount);

		FLLMAllocator* Allocator;

		FCriticalSection TagSection;
		FLLMArray<int64> TagStack;
#if LLM_ALLOW_ASSETS_TAGS
		FLLMArray<int64> AssetTagStack;
#endif
		FLLMArray<int64> TaggedAllocs;
#if LLM_TRACK_PEAK_MEMORY
		FLLMArray<int64> TaggedAllocPeaks;
#endif
		FLLMArray<int64> TaggedAllocTags;
		int64 UntaggedAllocs;
#if LLM_TRACK_PEAK_MEMORY
		int64 UntaggedAllocsPeak;
#endif
		bool bPaused;
	};

	FLLMThreadState* GetOrCreateState();
	FLLMThreadState* GetState();

	FLLMAllocator* Allocator;

	uint32 TlsSlot;

	FCriticalSection ThreadArraySection;
	FLLMObjectAllocator<FLLMThreadState> ThreadStateAllocator;
	FLLMArray<FLLMThreadState*> ThreadStates;

	int64 TrackedMemoryOverFrames GCC_ALIGN(8);

	LLMMap AllocationMap;

	ELLMTag UntaggedTotalTag;
	ELLMTag TrackedTotalTag;

	FLLMThreadState LocalState;

	FLLMCsvWriter CsvWriter;

	double LastTrimTime;
};

static int64 FNameToTag(FName Name)
{
	if (Name == NAME_None)
	{
		return (int64)ELLMTag::Untagged;
	}

	// get the bits out of the FName we need
	int64 NameIndex = Name.GetComparisonIndex();
	int64 NameNumber = Name.GetNumber();
	int64 tag = (NameNumber << 32) | NameIndex;
	checkf(tag > (int64)ELLMTag::PlatformTagEnd, TEXT("Passed with a name index [%d - %s] that was less than MemTracker_MaxUserAllocation"), NameIndex, *Name.ToString());

	// convert it to a tag, but you can actually convert this to an FMinimalName in the debugger to view it - *((FMinimalName*)&Tag)
	return tag;
}

static FName TagToFName(int64 Tag)
{
	// pull the bits back out of the tag
	int32 NameIndex = (int32)(Tag & 0xFFFFFFFF);
	int32 NameNumber = (int32)(Tag >> 32);
	return FName(NameIndex, NameIndex, NameNumber);
}

FLowLevelMemTracker& FLowLevelMemTracker::Get()
{
	static FLowLevelMemTracker Tracker;
	return Tracker;
}

bool FLowLevelMemTracker::IsEnabled()
{
	return !bIsDisabled;
}

FLowLevelMemTracker::FLowLevelMemTracker()
	: bFirstTimeUpdating(true)
	, bIsDisabled(false)		// must start off enabled because alllocations happen before the command line enables/disables us
	, bCanEnable(true)
	, bCsvWriterEnabled(false)
	, bInitialisedTrackers(false)
{
	// set the LLMMap alloc functions
	LLMAllocFunction PlatformLLMAlloc = NULL;
	LLMFreeFunction PlatformLLMFree = NULL;
	int32 Alignment = 0;
	if (!FPlatformMemory::GetLLMAllocFunctions(PlatformLLMAlloc, PlatformLLMFree, Alignment))
	{
		bIsDisabled = true;
		bCanEnable = false;
	}

	Allocator.Initialise(PlatformLLMAlloc, PlatformLLMFree, Alignment);

	// only None is on by default
	for (int32 Index = 0; Index < (int32)ELLMTagSet::Max; Index++)
	{
		ActiveSets[Index] = Index == (int32)ELLMTagSet::None;
	}
}

FLowLevelMemTracker::~FLowLevelMemTracker()
{
	for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; ++TrackerIndex)
	{
		Trackers[TrackerIndex]->~FLLMTracker();
		Allocator.Free(Trackers[TrackerIndex], sizeof(FLowLevelMemTracker));
	}
}

void FLowLevelMemTracker::InitialiseTrackers()
{
	for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; ++TrackerIndex)
	{
		FLLMTracker* Tracker = (FLLMTracker*)Allocator.Alloc(sizeof(FLLMTracker));
		new (Tracker)FLLMTracker();

		Trackers[TrackerIndex] = Tracker;

		Tracker->Initialise((ELLMTracker)TrackerIndex, &Allocator);
	}

	// calculate program size early on... the platform can call update the program size later if it sees fit
	InitialiseProgramSize();
}

void FLowLevelMemTracker::UpdateStatsPerFrame(const TCHAR* LogName)
{
	if (bIsDisabled && !bCanEnable)
		return;

	// let some stats get through even if we've disabled LLM - this shows up some overhead that is always there even when disabled
	// (unless the #define completely removes support, of course)
	if (bIsDisabled && !bFirstTimeUpdating)
	{
		return;
	}

	// delay init
	if (bFirstTimeUpdating)
	{
		static_assert((uint8)ELLMTracker::Max == 2, "You added a tracker, without updating FLowLevelMemTracker::UpdateStatsPerFrame (and probably need to update macros)");

		GetTracker(ELLMTracker::Platform)->SetTotalTags(ELLMTag::PlatformUntrackedTotal, ELLMTag::PlatformTrackedTotal);
		GetTracker(ELLMTracker::Default)->SetTotalTags(ELLMTag::UntrackedTotal, ELLMTag::TrackedTotal);

		bFirstTimeUpdating = false;
	}

	int64 TrackedProcessMemory = 0;
	for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; TrackerIndex++)
	{
		FLLMTracker* Tracker = GetTracker((ELLMTracker)TrackerIndex);

		// update stats and also get how much memory is now tracked
		int64 TrackedMemory = Tracker->UpdateFrameAndReturnTotalTrackedMemory(PlatformTags);

		// the Platform layer is special in that we use it to track untracked memory (it's expected that every other allocation
		// goes through this level, and if not, there's nothing better we can do)
		if (TrackerIndex == (int32)ELLMTracker::Platform)
		{
			TrackedProcessMemory = TrackedMemory;
		}
	}

	// set overhead stats
	int64 StaticOverhead = sizeof(FLowLevelMemTracker) + sizeof(FLLMTracker) * (int32)ELLMTracker::Max;
	int64 Overhead = StaticOverhead + Allocator.GetTotal();
	SET_MEMORY_STAT(STAT_LLMOverheadTotal, Overhead);

	// calculate memory the platform thinks we have allocated, compared to what we have tracked, including the program memory
	FPlatformMemoryStats PlatformStats = FPlatformMemory::GetStats();
	uint64 PlatformProcessMemory = PlatformStats.TotalPhysical - PlatformStats.AvailablePhysical - Overhead;
	int64 PlatformTotalUntracked = PlatformProcessMemory - TrackedProcessMemory;
	SET_MEMORY_STAT(STAT_LLMPlatformTotal, PlatformProcessMemory);
	SET_MEMORY_STAT(STAT_LLMPlatformTotalUntracked, PlatformTotalUntracked);

	if (bCsvWriterEnabled)
	{
		for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; ++TrackerIndex)
		{
			GetTracker((ELLMTracker)TrackerIndex)->WriteCsv(PlatformTags);
		}
	}

	if (LogName != nullptr)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("---> Untracked memory at %s = %.2f mb\n"), LogName, (double)PlatformTotalUntracked / (1024.0 * 1024.0));
	}
}

void FLowLevelMemTracker::InitialiseProgramSize()
{
	if (!ProgramSize)
	{
		FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		ProgramSize = Stats.TotalPhysical - Stats.AvailablePhysical;

		Trackers[(int32)ELLMTracker::Platform]->TrackMemory((uint64)ELLMTag::ProgramSizePlatform, ProgramSize);
		Trackers[(int32)ELLMTracker::Default]->TrackMemory((uint64)ELLMTag::ProgramSize, ProgramSize);
	}
}

void FLowLevelMemTracker::SetProgramSize(uint64 InProgramSize)
{
	if (bIsDisabled)
		return;

	int64 ProgramSizeDiff = InProgramSize - ProgramSize;

	ProgramSize = InProgramSize;

	GetTracker(ELLMTracker::Platform)->TrackMemory((uint64)ELLMTag::ProgramSizePlatform, ProgramSizeDiff);
	GetTracker(ELLMTracker::Default)->TrackMemory((uint64)ELLMTag::ProgramSize, ProgramSizeDiff);
}

void FLowLevelMemTracker::ProcessCommandLine(const TCHAR* CmdLine)
{
	if (bIsDisabled && !bCanEnable)
		return;

	if (bCanEnable)
	{
#if LLM_COMMANDLINE_ENABLES_FUNCTIONALITY
		// if we require commandline to enable it, then we are disabled if it's not there
		bIsDisabled = FParse::Param(CmdLine, TEXT("LLM")) == false;
#else
		// if we allow commandline to disable us, then we are disabled if it's there
		bIsDisabled = FParse::Param(CmdLine, TEXT("NOLLM")) == true;
#endif
	}

	bCsvWriterEnabled = FParse::Param(CmdLine, TEXT("LLMCSV"));
	for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; ++TrackerIndex)
	{
		GetTracker((ELLMTracker)TrackerIndex)->SetCSVEnabled(bCsvWriterEnabled);
	}

	// automatically enable LLM if only LLMCSV is there
	if (bCsvWriterEnabled && bIsDisabled && bCanEnable)
	{
		bIsDisabled = false;
	}

	if (bIsDisabled)
	{
		for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; TrackerIndex++)
		{
			GetTracker((ELLMTracker)TrackerIndex)->Clear();
		}
	}

	// activate tag sets (we ignore None set, it's always on)
	FString SetList;
	static_assert((uint8)ELLMTagSet::Max == 3, "You added a tagset, without updating FLowLevelMemTracker::ProcessCommandLine");
	if (FParse::Value(CmdLine, TEXT("LLMTAGSETS="), SetList))
	{
		TArray<FString> Sets;
		SetList.ParseIntoArray(Sets, TEXT(","), true);
		for (FString& Set : Sets)
		{
			if (Set == TEXT("Assets"))
			{
#if LLM_ALLOW_ASSETS_TAGS // asset tracking has a per-thread memory overhead, so we have a #define to completely disable it - warn if we don't match
				ActiveSets[(int32)ELLMTagSet::Assets] = true;
#else
				UE_LOG(LogInit, Warning, TEXT("Attempted to use LLM to track assets, but LLM_ALLOW_ASSETS_TAGS is not defined to 1. You will need to enable the define"));
#endif
			}
			else if (Set == TEXT("AssetClasses"))
			{
				ActiveSets[(int32)ELLMTagSet::AssetClasses] = true;
			}
		}
	}
}

void FLowLevelMemTracker::OnLowLevelAlloc(ELLMTracker Tracker, const void* Ptr, uint64 Size, ELLMTag DefaultTag)
{
	if (bIsDisabled)
	{
		return;
	}

	GetTracker(Tracker)->TrackAllocation(Ptr, Size, DefaultTag, Tracker);
}

void FLowLevelMemTracker::OnLowLevelFree(ELLMTracker Tracker, const void* Ptr, uint64 CheckSize)
{
	if (bIsDisabled)
	{
		return;
	}

	if (Ptr != nullptr)
	{
		GetTracker(Tracker)->TrackFree(Ptr, CheckSize, Tracker);
	}
}

FLLMTracker* FLowLevelMemTracker::GetTracker(ELLMTracker Tracker)
{
	if (!bInitialisedTrackers)
	{
		InitialiseTrackers();
		bInitialisedTrackers = true;
	}

	return Trackers[(int32)Tracker];
}

void FLowLevelMemTracker::OnLowLevelAllocMoved(ELLMTracker Tracker, const void* Dest, const void* Source)
{
	if (bIsDisabled)
	{
		return;
	}

	GetTracker(Tracker)->OnAllocMoved(Dest, Source);
}

bool FLowLevelMemTracker::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("LLMEM")))
	{
		if (FParse::Command(&Cmd, TEXT("SPAMALLOC")))
		{
			int32 NumAllocs = 128;
			int64 MaxSize = FCString::Atoi(Cmd);
			if (MaxSize == 0)
			{
				MaxSize = 128 * 1024;
			}

			UpdateStatsPerFrame(TEXT("Before spam"));
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("----> Spamming %d allocations, from %d..%d bytes\n"), NumAllocs, MaxSize/2, MaxSize);

			TArray<void*> Spam;
			Spam.Reserve(NumAllocs);
			uint32 TotalSize = 0;
			for (int32 Index = 0; Index < NumAllocs; Index++)
			{
				int32 Size = (FPlatformMath::Rand() % MaxSize / 2) + MaxSize / 2;
				TotalSize += Size;
				Spam.Add(FMemory::Malloc(Size));
			}
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("----> Allocated %d total bytes\n"), TotalSize);

			UpdateStatsPerFrame(TEXT("After spam"));

			for (int32 Index = 0; Index < Spam.Num(); Index++)
			{
				FMemory::Free(Spam[Index]);
			}

			UpdateStatsPerFrame(TEXT("After cleanup"));
		}
		return true;
	}

	return false;
}

bool FLowLevelMemTracker::IsTagSetActive(ELLMTagSet Set)
{
	return !bIsDisabled && ActiveSets[(int32)Set];
}

bool FLowLevelMemTracker::ShouldReduceThreads()
{
	return IsTagSetActive(ELLMTagSet::Assets) || IsTagSetActive(ELLMTagSet::AssetClasses);
}

static bool IsAssetTagForAssets(ELLMTagSet Set)
{
	return Set == ELLMTagSet::Assets || Set == ELLMTagSet::AssetClasses;
}

void FLowLevelMemTracker::RegisterPlatformTag(int32 Tag, const TCHAR* Name, FName StatName, FName SummaryStatName)
{
	check(Tag >= (int32)ELLMTag::PlatformTagStart && Tag <= (int32)ELLMTag::PlatformTagEnd);
	FLLMPlatformTag& PlatformTag = PlatformTags[Tag - (int32)ELLMTag::PlatformTagStart];
	PlatformTag.Tag = Tag;
	PlatformTag.Name = Name;
	PlatformTag.StatName = StatName;
	PlatformTag.SummaryStatName = SummaryStatName;
}



FLLMScopedTag::FLLMScopedTag(FName StatIDName, ELLMTagSet Set, ELLMTracker Tracker)
{
	Init(FNameToTag(StatIDName), Set, Tracker);
}

FLLMScopedTag::FLLMScopedTag(ELLMTag Tag, ELLMTagSet Set, ELLMTracker Tracker)
{
	Init((int64)Tag, Set, Tracker);
}

void FLLMScopedTag::Init(int64 Tag, ELLMTagSet Set, ELLMTracker Tracker)
{
	TagSet = Set;
	TrackerSet = Tracker;
	Enabled = Tag != (int64)ELLMTag::Untagged;

	// early out if tracking is disabled (don't do the singleton call, this is called a lot!)
	if (!Enabled)
	{
		return;
	}

	FLowLevelMemTracker& LLM = FLowLevelMemTracker::Get();
	if (!LLM.IsTagSetActive(TagSet))
	{
		return;
	}

#if LLM_ALLOW_ASSETS_TAGS
	if (IsAssetTagForAssets(TagSet))
	{
		LLM.GetTracker(Tracker)->PushAssetTag(Tag);
	}
	else
#endif
	{
		LLM.GetTracker(Tracker)->PushTag(Tag);
	}
}

FLLMScopedTag::~FLLMScopedTag()
{
	// early out if tracking is disabled (don't do the singleton call, this is called a lot!)
	if (!Enabled)
	{
		return;
	}

	FLowLevelMemTracker& LLM = FLowLevelMemTracker::Get();
	if (!LLM.IsTagSetActive(TagSet))
	{
		return;
	}

#if LLM_ALLOW_ASSETS_TAGS
	if (IsAssetTagForAssets(TagSet))
	{
		LLM.GetTracker(TrackerSet)->PopAssetTag();
	}
	else
#endif
	{
		LLM.GetTracker(TrackerSet)->PopTag();
	}
}


FLLMScopedPauseTrackingWithAmountToTrack::FLLMScopedPauseTrackingWithAmountToTrack(FName StatIDName, int64 Amount, ELLMTracker TrackerToPause)
{
	Init(FNameToTag(StatIDName), Amount, TrackerToPause);
}

FLLMScopedPauseTrackingWithAmountToTrack::FLLMScopedPauseTrackingWithAmountToTrack(ELLMTag Tag, int64 Amount, ELLMTracker TrackerToPause)
{
	Init((uint64)Tag, Amount, TrackerToPause);
}

void FLLMScopedPauseTrackingWithAmountToTrack::Init(int64 Tag, int64 Amount, ELLMTracker TrackerToPause)
{
	FLowLevelMemTracker& LLM = FLowLevelMemTracker::Get();
	if (!LLM.IsTagSetActive(ELLMTagSet::None))
	{
		return;
	}

	for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; TrackerIndex++)
	{
		ELLMTracker Tracker = (ELLMTracker)TrackerIndex;

		if (TrackerToPause == ELLMTracker::Max || TrackerToPause == Tracker)
		{
			if (Amount == 0)
			{
				LLM.GetTracker((ELLMTracker)TrackerIndex)->Pause();
			}
			else
			{
				LLM.GetTracker((ELLMTracker)TrackerIndex)->PauseAndTrackMemory(Tag, Amount);
			}
		}
	}
}

FLLMScopedPauseTrackingWithAmountToTrack::~FLLMScopedPauseTrackingWithAmountToTrack()
{
	FLowLevelMemTracker& LLM = FLowLevelMemTracker::Get();
	if (!LLM.IsTagSetActive(ELLMTagSet::None))
	{
		return;
	}

	for (int32 TrackerIndex = 0; TrackerIndex < (int32)ELLMTracker::Max; TrackerIndex++)
	{
		LLM.GetTracker((ELLMTracker)TrackerIndex)->Unpause();
	}
}





FLLMTracker::FLLMTracker()
	: TrackedMemoryOverFrames(0)
	, UntaggedTotalTag(ELLMTag::Untagged)
	, TrackedTotalTag(ELLMTag::Untagged)
	, LastTrimTime(0.0)

{
	TlsSlot = FPlatformTLS::AllocTlsSlot();
}

FLLMTracker::~FLLMTracker()
{
	Clear();

	FPlatformTLS::FreeTlsSlot(TlsSlot);
}

void FLLMTracker::Initialise(
	ELLMTracker Tracker,
	FLLMAllocator* InAllocator)
{
	CsvWriter.SetTracker(Tracker);

	Allocator = InAllocator;

	AllocationMap.SetAllocator(InAllocator);

	LocalState.SetAllocator(InAllocator);

	CsvWriter.SetAllocator(InAllocator);

	ThreadStateAllocator.SetAllocator(Allocator);
	ThreadStates.SetAllocator(Allocator);
}

FLLMTracker::FLLMThreadState* FLLMTracker::GetOrCreateState()
{
	// look for already allocated thread state
	FLLMThreadState* State = (FLLMThreadState*)FPlatformTLS::GetTlsValue(TlsSlot);
	// get one if needed
	if (State == nullptr)
	{
		// protect any accesses to the ThreadStates array
		FScopeLock Lock(&ThreadArraySection);

		State = ThreadStateAllocator.New();
		State->SetAllocator(Allocator);
		ThreadStates.Add(State);

		// push to Tls
		FPlatformTLS::SetTlsValue(TlsSlot, State);
	}
	return State;
}

FLLMTracker::FLLMThreadState* FLLMTracker::GetState()
{
	return (FLLMThreadState*)FPlatformTLS::GetTlsValue(TlsSlot);
}

void FLLMTracker::PushTag(int64 Tag)
{
	// pass along to the state object
	GetOrCreateState()->PushTag(Tag);
}

void FLLMTracker::PopTag()
{
	// look for already allocated thread state
	FLLMThreadState* State = GetState();

	checkf(State != nullptr, TEXT("Called PopTag but PushTag was never called!"));

	State->PopTag();
}

#if LLM_ALLOW_ASSETS_TAGS
void FLLMTracker::PushAssetTag(int64 Tag)
{
	// pass along to the state object
	GetOrCreateState()->PushAssetTag(Tag);
}

void FLLMTracker::PopAssetTag()
{
	// look for already allocated thread state
	FLLMThreadState* State = GetState();

	checkf(State != nullptr, TEXT("Called PopTag but PushTag was never called!"));

	State->PopAssetTag();
}
#endif

void FLLMTracker::TrackAllocation(const void* Ptr, uint64 Size, ELLMTag DefaultTag, ELLMTracker Tracker)
{
	if (IsPaused())
	{
		return;
	}

	// track the total quickly
	FPlatformAtomics::InterlockedAdd(&TrackedMemoryOverFrames, (int64)Size);
	
	FLLMThreadState* State = GetOrCreateState();
	
	// track on the thread state, and get the tag
	State->TrackAllocation(Ptr, Size, DefaultTag, Tracker);

	// tracking a nullptr with a Size is allowed, but we don't need to remember it, since we can't free it ever
	if (Ptr != nullptr)
	{
		// remember the size and tag info
		int64 tag = State->GetTopTag();
		if (tag == (int64)ELLMTag::Untagged)
			tag = (int64)DefaultTag;

		FLLMTracker::FLowLevelAllocInfo AllocInfo;
		#if LLM_USE_ALLOC_INFO_STRUCT
			AllocInfo.Tag = tag;
			#if LLM_ALLOW_ASSETS_TAGS
				AllocInfo.AssetTag = State->GetTopAssetTag();
			#endif
		#else
			check(tag >= 0 && tag <= (int64)ELLMTag::PlatformTagEnd);
			AllocInfo = (ELLMTag)tag;
		#endif

		check(Size <= 0xffffffff);
		GetAllocationMap().Add(Ptr, Size, AllocInfo);
	}
}

void FLLMTracker::TrackFree(const void* Ptr, uint64 CheckSize, ELLMTracker Tracker)
{
	if (IsPaused())
	{
		return;
	}

	// look up the pointer in the tracking map
	LLMMap::Values Values = GetAllocationMap().Remove(Ptr);
	uint64 Size = Values.Value1;
	FLLMTracker::FLowLevelAllocInfo AllocInfo = Values.Value2;

	// track the total quickly
	FPlatformAtomics::InterlockedAdd(&TrackedMemoryOverFrames, 0 - Size);

	FLLMThreadState* State = GetOrCreateState();

#if LLM_USE_ALLOC_INFO_STRUCT
	State->TrackFree(Ptr, AllocInfo.Tag, Size, true, Tracker);
	#if LLM_ALLOW_ASSETS_TAGS
		State->TrackFree(nullptr, AllocInfo.AssetTag, Size, false, Tracker);
	#endif
#else
	State->TrackFree(Ptr, (int64)AllocInfo, Size, true, Tracker);
#endif

	// CheckSize is just used to verify (at least for now)
	checkf(CheckSize == 0 || CheckSize == Size, TEXT("Called LLM::OnFree with a Size, but it didn't match what was allocated? [allocated = %lld, passed in = %lld]"), Size, CheckSize);
}

void FLLMTracker::OnAllocMoved(const void* Dest, const void* Source)
{
	LLMMap::Values Values = GetAllocationMap().Remove(Source);
	GetAllocationMap().Add(Dest, Values.Value1, Values.Value2);
}

void FLLMTracker::TrackMemory(int64 Tag, int64 Amount)
{
	FLLMTracker::FLLMThreadState* State = GetOrCreateState();
	State->IncrTag(Tag, Amount, true);
	FPlatformAtomics::InterlockedAdd(&TrackedMemoryOverFrames, Amount);
}

// This will pause/unpause tracking, and also manually increment a given tag
void FLLMTracker::PauseAndTrackMemory(int64 Tag, int64 Amount)
{
	FLLMTracker::FLLMThreadState* State = GetOrCreateState();
	State->bPaused = true;
	State->IncrTag(Tag, Amount, true);
	FPlatformAtomics::InterlockedAdd(&TrackedMemoryOverFrames, Amount);
}

void FLLMTracker::Pause()
{
	FLLMTracker::FLLMThreadState* State = GetOrCreateState();
	State->bPaused = true;
}

void FLLMTracker::Unpause()
{
	GetOrCreateState()->bPaused = false;
}

bool FLLMTracker::IsPaused()
{
	FLLMTracker::FLLMThreadState* State = GetState();
	// pause during shutdown, as the massive number of frees is likely to overflow some of the buffers
	return GIsRequestingExit || (State == nullptr ? false : State->bPaused);
}

void FLLMTracker::Clear()
{
	for (int32 Index = 0; Index < ThreadStates.Num(); ++Index)
		ThreadStateAllocator.Delete(ThreadStates[Index]);
	ThreadStates.Clear(true);

	AllocationMap.Clear();
	CsvWriter.Clear();
	ThreadStateAllocator.Clear();
}

void FLLMTracker::SetCSVEnabled(bool Value)
{
	CsvWriter.SetEnabled(Value);
}

void FLLMTracker::SetTotalTags(ELLMTag InUntaggedTotalTag, ELLMTag InTrackedTotalTag)
{
	UntaggedTotalTag = InUntaggedTotalTag;
	TrackedTotalTag = InTrackedTotalTag;
}

uint64 FLLMTracker::UpdateFrameAndReturnTotalTrackedMemory(FLLMPlatformTag* PlatformTags)
{
	// protect any accesses to the ThreadStates array
	FScopeLock Lock(&ThreadArraySection);

	int ThreadStateNum = ThreadStates.Num();
	for (int32 ThreadIndex = 0; ThreadIndex < ThreadStateNum; ThreadIndex++)
	{
		// update stat per thread states
		ThreadStates[ThreadIndex]->UpdateFrame(UntaggedTotalTag, LocalState, CsvWriter, PlatformTags);
	}

	FName StatName = ELLMTagNames[(int32)TrackedTotalTag].StatName;
	if (StatName != NAME_None)
	{
		SET_MEMORY_STAT_FName(StatName, TrackedMemoryOverFrames);
	}

	FName SummaryStatName = ELLMTagNames[(int32)TrackedTotalTag].SummaryStatName;
	if (SummaryStatName != NAME_None)
	{
		SET_MEMORY_STAT_FName(SummaryStatName, TrackedMemoryOverFrames);
	}

#if LLM_TRACK_PEAK_MEMORY
	// @todo we should be keeping track of the intra-frame memory peak for the total tracked memory.
	// For now we will just use the memory at the time the update happens since there are threading implications to being accurate.
	CsvWriter.SetStat(FNameToTag(TrackedTotalTag), TrackedMemoryOverFrames, TrackedMemoryOverFrames);
#else
	CsvWriter.SetStat((int64)TrackedTotalTag, TrackedMemoryOverFrames);
#endif

	if (FPlatformTime::Seconds() - LastTrimTime > 10)
	{
		AllocationMap.Trim();
		LastTrimTime = FPlatformTime::Seconds();
	}

	return (uint64)TrackedMemoryOverFrames;
}

void FLLMTracker::WriteCsv(FLLMPlatformTag* PlatformTags)
{
	CsvWriter.Update(PlatformTags);
}


FLLMTracker::FLLMThreadState::FLLMThreadState()
	: UntaggedAllocs(0)
	, bPaused(false)
{
}

void FLLMTracker::FLLMThreadState::SetAllocator(FLLMAllocator* InAllocator)
{
	TagStack.SetAllocator(InAllocator);

#if LLM_ALLOW_ASSETS_TAGS
	AssetTagStack.SetAllocator(InAllocator);
#endif

	TaggedAllocs.SetAllocator(InAllocator);
#if LLM_TRACK_PEAK_MEMORY
	TaggedAllocPeaks.SetAllocator(InAllocator);
#endif
	TaggedAllocTags.SetAllocator(InAllocator);
}

void FLLMTracker::FLLMThreadState::Clear()
{
	TagStack.Clear();

#if LLM_ALLOW_ASSETS_TAGS
	AssetTagStack.Clear();
#endif

	TaggedAllocs.Clear();
#if LLM_TRACK_PEAK_MEMORY
	TaggedAllocPeaks.Clear();
#endif
	TaggedAllocTags.Clear();
}

void FLLMTracker::FLLMThreadState::PushTag(int64 Tag)
{
	FScopeLock Lock(&TagSection);

	// push a tag
	TagStack.Add(Tag);
}

void FLLMTracker::FLLMThreadState::PopTag()
{
	FScopeLock Lock(&TagSection);

	checkf(TagStack.Num() > 0, TEXT("Called FLLMTracker::FLLMThreadState::PopTag without a matching Push (stack was empty on pop)"));
	TagStack.RemoveLast();
}

int64 FLLMTracker::FLLMThreadState::GetTopTag()
{
	// make sure we have some pushed
	if (TagStack.Num() == 0)
	{
		return (int64)ELLMTag::Untagged;
	}

	// return the top tag
	return TagStack.GetLast();
}

#if LLM_ALLOW_ASSETS_TAGS
void FLLMTracker::FLLMThreadState::PushAssetTag(int64 Tag)
{
	FScopeLock Lock(&TagSection);

	// push a tag
	AssetTagStack.Add(Tag);
}

void FLLMTracker::FLLMThreadState::PopAssetTag()
{
	FScopeLock Lock(&TagSection);

	checkf(AssetTagStack.Num() > 0, TEXT("Called FLLMTracker::FLLMThreadState::PopTag without a matching Push (stack was empty on pop)"));
	AssetTagStack.RemoveLast();
}

int64 FLLMTracker::FLLMThreadState::GetTopAssetTag()
{
	// make sure we have some pushed
	if (AssetTagStack.Num() == 0)
	{
		return (int64)ELLMTag::Untagged;
	}

	// return the top tag
	return AssetTagStack.GetLast();
}
#endif

void FLLMTracker::FLLMThreadState::IncrTag(int64 Tag, int64 Amount, bool bTrackUntagged)
{
	// track the untagged allocations
	if (Tag == (int64)ELLMTag::Untagged)
	{
		if (bTrackUntagged)
		{
			UntaggedAllocs += Amount;
#if LLM_TRACK_PEAK_MEMORY
			if (UntaggedAllocs > UntaggedAllocsPeak)
			{
				UntaggedAllocsPeak = UntaggedAllocs;
			}
#endif
		}
	}
	else
	{
		// look over existing tags on this thread for already tracking this tag
		for (int32 TagSearch = 0; TagSearch < TaggedAllocTags.Num(); TagSearch++)
		{
			if (TaggedAllocTags[TagSearch] == Tag)
			{
				// update it if we found it, and break out
				TaggedAllocs[TagSearch] += Amount;
#if LLM_TRACK_PEAK_MEMORY
				if (TaggedAllocs[TagSearch] > TaggedAllocPeaks[TagSearch])
				{
					TaggedAllocPeaks[TagSearch] = TaggedAllocs[TagSearch];
				}
#endif
				return;
			}
		}

		// if we get here, then we need to add a new tracked tag
		TaggedAllocTags.Add(Tag);
		TaggedAllocs.Add(Amount);
#if LLM_TRACK_PEAK_MEMORY
		TaggedAllocPeaks.Add(Amount);
#endif
	}
}

void FLLMTracker::FLLMThreadState::TrackAllocation(const void* Ptr, uint64 Size, ELLMTag DefaultTag, ELLMTracker Tracker)
{
	FScopeLock Lock(&TagSection);

	int64 Tag = GetTopTag();
	if (Tag == (int64)ELLMTag::Untagged)
		Tag = (int64)DefaultTag;
	IncrTag(Tag, Size, true);
#if LLM_ALLOW_ASSETS_TAGS
	int64 AssetTag = GetTopAssetTag();
	IncrTag(AssetTag, Size, false);
#endif
	
#if ENABLE_MEMPRO
	if (START_MEMPRO && Tracker == ELLMTracker::Default && (MemProTrackTag == ELLMTag::MaxUserAllocation || MemProTrackTag == (ELLMTag)Tag))
	{
		MEMPRO_TRACK_ALLOC((void*)Ptr, (size_t)Size);
	}
#endif
}

void FLLMTracker::FLLMThreadState::TrackFree(const void* Ptr, int64 Tag, uint64 Size, bool bTrackedUntagged, ELLMTracker Tracker)
{
	FScopeLock Lock(&TagSection);

	IncrTag(Tag, 0 - Size, bTrackedUntagged);

#if ENABLE_MEMPRO
	if (START_MEMPRO && Tracker == ELLMTracker::Default && (MemProTrackTag == ELLMTag::MaxUserAllocation || MemProTrackTag == (ELLMTag)Tag))
	{
		MEMPRO_TRACK_FREE((void*)Ptr);
	}
#endif
}

void FLLMTracker::FLLMThreadState::UpdateFrame(
	ELLMTag InUntaggedTotalTag,
	FLLMThreadState& InLocalState,
	FLLMCsvWriter& InCsvWriter,
	FLLMPlatformTag* PlatformTags)
{
	// grab the stats in a thread safe way
	{
		FScopeLock Lock(&TagSection);

		InLocalState.UntaggedAllocs = UntaggedAllocs;

		InLocalState.TaggedAllocTags = TaggedAllocTags;
		InLocalState.TaggedAllocs = TaggedAllocs;
#if LLM_TRACK_PEAK_MEMORY
		InLocalState.TaggedAllocPeaks = TaggedAllocPeaks;
#endif
		// restart the tracking now that we've copied out, safely
		UntaggedAllocs = 0;
		TaggedAllocTags.Clear();
		TaggedAllocs.Clear();
#if LLM_TRACK_PEAK_MEMORY
		TaggedAllocPeaks.Clear();
		UntaggedAllocsPeak = 0;
#endif
	}

	IncMemoryStatByFName(ELLMTagNames[(int32)InUntaggedTotalTag].StatName, InLocalState.UntaggedAllocs);
	IncMemoryStatByFName(ELLMTagNames[(int32)InUntaggedTotalTag].SummaryStatName, InLocalState.UntaggedAllocs);

#if LLM_TRACK_PEAK_MEMORY
	InCsvWriter.AddStat(FNameToTag(InUntaggedStatName), InLocalState.UntaggedAllocs, InLocalState.UntaggedAllocsPeak);
#else
	InCsvWriter.AddStat((int64)InUntaggedTotalTag, InLocalState.UntaggedAllocs);
#endif

	// walk over the tags for this level
	for (int32 TagIndex = 0; TagIndex < InLocalState.TaggedAllocTags.Num(); TagIndex++)
	{
		int64 Tag = InLocalState.TaggedAllocTags[TagIndex];
		int64 Amount = InLocalState.TaggedAllocs[TagIndex];

		//---------------------
		// update csv
#if LLM_TRACK_PEAK_MEMORY
		int64 Peak = InLocalState.TaggedAllocPeaks[TagIndex];
		InCsvWriter.AddStat(Tag, Amount, Peak);
#else
		InCsvWriter.AddStat(Tag, Amount);
#endif

		//---------------------
		// update the stats
		if (Tag > (int64)ELLMTag::PlatformTagEnd)
		{
			IncMemoryStatByFName(TagToFName(Tag), Amount);
		}
		else if (Tag >= (int64)ELLMTag::PlatformTagStart)
		{
			IncMemoryStatByFName(PlatformTags[Tag - (int32)ELLMTag::PlatformTagStart].StatName, int64(Amount));
			IncMemoryStatByFName(PlatformTags[Tag - (int32)ELLMTag::PlatformTagStart].SummaryStatName, int64(Amount));
		}
		else
		{
			check(Tag >= 0 && Tag < sizeof(ELLMTagNames) / sizeof(FLLMTagInfo));
			IncMemoryStatByFName(ELLMTagNames[Tag].StatName, int64(Amount));
			IncMemoryStatByFName(ELLMTagNames[Tag].SummaryStatName, int64(Amount));
		}
	}

	InLocalState.Clear();
}

void FLLMTracker::FLLMThreadState::IncMemoryStatByFName(FName Name, int64 Amount)
{
	if (Name != NAME_None)
	{
		INC_MEMORY_STAT_BY_FName(Name, Amount);
	}
}

/*
 * FLLMCsvWriter implementation
*/

/*
* don't allocate memory in the constructor because it is called before allocators are setup
*/
FLLMCsvWriter::FLLMCsvWriter()
	: Enabled(true)
	, WriteCount(0)
	, LastWriteTime(FPlatformTime::Seconds())
	, Archive(NULL)
	, LastWriteStatValueCount(0)
{
}

FLLMCsvWriter::~FLLMCsvWriter()
{
	delete Archive;
}

void FLLMCsvWriter::Clear()
{
	StatValues.Clear(true);
	StatValuesForWrite.Clear(true);
}

/*
* don't allocate memory in this function because it is called by the allocator
*/
#if LLM_TRACK_PEAK_MEMORY
void FLLMCsvWriter::AddStat(int64 Tag, int64 Value, int64 Peak)
#else
void FLLMCsvWriter::AddStat(int64 Tag, int64 Value)
#endif
{
	FScopeLock lock(&StatValuesLock);

	if (!Enabled)
	{
		return;
	}

	int StatValueCount = StatValues.Num();
	for (int32 i = 0; i < StatValueCount; ++i)
	{
		if (StatValues[i].Tag == Tag)
		{
#if LLM_TRACK_PEAK_MEMORY
			int64 PossibleNewPeak = StatValues[i].Value + Peak;
			if (PossibleNewPeak > StatValues[i].Peak)
			{
				StatValues[i].Peak = PossibleNewPeak;
			}
#endif
			StatValues[i].Value += Value;
			return;
		}
	}

	StatValue NewStatValue;
	NewStatValue.Tag = Tag;
	NewStatValue.Value = Value;
#if LLM_TRACK_PEAK_MEMORY
	NewStatValue.Peak = Peak;
#endif
	StatValues.Add(NewStatValue);
}

/*
* don't allocate memory in this function because it is called by the allocator
*/
#if LLM_TRACK_PEAK_MEMORY
void FLLMCsvWriter::SetStat(int64 Tag, int64 Value, int64 Peak)
#else
void FLLMCsvWriter::SetStat(int64 Tag, int64 Value)
#endif
{
	FScopeLock lock(&StatValuesLock);

	int StatValueCount = StatValues.Num();
	for (int32 i = 0; i < StatValueCount; ++i)
	{
		if (StatValues[i].Tag == Tag)
		{
#if LLM_TRACK_PEAK_MEMORY
			if (Peak > StatValues[i].Peak)
			{
				StatValues[i].Peak = Peak;
			}
#endif
			StatValues[i].Value = Value;
			return;
		}
	}

	StatValue NewStatValue;
	NewStatValue.Tag = Tag;
	NewStatValue.Value = Value;
#if LLM_TRACK_PEAK_MEMORY
	NewStatValue.Peak = Peak;
#endif
	StatValues.Add(NewStatValue);
}

/*
* memory can be allocated in this function
*/
void FLLMCsvWriter::Update(FLLMPlatformTag* PlatformTags)
{
	double Now = FPlatformTime::Seconds();
	if (Now - LastWriteTime >= (double)CVarLLMWriteInterval.GetValueOnGameThread())
	{
		WriteGraph(PlatformTags);

		LastWriteTime = Now;
	}
}

const TCHAR* FLLMCsvWriter::GetTrackerCsvName(ELLMTracker InTracker)
{
	switch (InTracker)
	{
		case ELLMTracker::Default: return TEXT("LLM");
		case ELLMTracker::Platform: return TEXT("LLMPlatform");
		default: check(false); return TEXT("");
	}
}

/*
 * Archive is a binary stream, so we can't just serialise an FString using <<
*/
void FLLMCsvWriter::Write(const FString& Text)
{
	Archive->Serialize((void*)*Text, Text.Len() * sizeof(TCHAR));
}

/*
 * create the csv file on the first call. When it finds a new stat name it seeks
 * back to the start of the file and re-writes the column names.
*/
void FLLMCsvWriter::WriteGraph(FLLMPlatformTag* PlatformTags)
{
	// create the csv file
	if (!Archive)
	{
		FString Directory = FPaths::ProfilingDir() + "LLM/";
		IFileManager::Get().MakeDirectory(*Directory, true);
		
		const TCHAR* TrackerName = GetTrackerCsvName(Tracker);
		const FDateTime FileDate = FDateTime::Now();
		FString Filename = FString::Printf(TEXT("%s/%s_%s.csv"), *Directory, TrackerName, *FileDate.ToString());
		Archive = IFileManager::Get().CreateFileWriter(*Filename);
		check(Archive);

		// create space for column titles that are filled in as we get them
		for (int32 i = 0; i < 500; ++i)
		{
			Write(TEXT("          "));
		}
		Write(TEXT("\n"));
	}

	// grab the stats (make sure that no allocations happen in this scope)
	{
		FScopeLock lock(&StatValuesLock);
		StatValuesForWrite = StatValues;
	}

	// re-write the column names if we have found a new one
	int32 StatValueCountLocal = StatValuesForWrite.Num();
	if (StatValueCountLocal != LastWriteStatValueCount)
	{
		int64 OriginalOffset = Archive->Tell();
		Archive->Seek(0);

		for (int32 i = 0; i < StatValueCountLocal; ++i)
		{
			FString StatName = GetTagName(StatValuesForWrite[i].Tag, PlatformTags);
			FString Text = FString::Printf(TEXT("%s,"), *StatName);
			Write(Text);
		}

		Archive->Seek(OriginalOffset);

		LastWriteStatValueCount = StatValueCountLocal;
	}

	// write the actual stats
	for (int32 i = 0; i < StatValueCountLocal; ++i)
	{
#if LLM_TRACK_PEAK_MEMORY
		FString Text = FString::Printf(TEXT("%0.2f,"), StatValuesForWrite[i].Peak / 1024.0f / 1024.0f);
#else
		FString Text = FString::Printf(TEXT("%0.2f,"), StatValuesForWrite[i].Value / 1024.0f / 1024.0f);
#endif
		Write(Text);
	}
	Write(TEXT("\n"));

	WriteCount++;

	if (CVarLLMWriteInterval.GetValueOnGameThread())
	{
		UE_LOG(LogHAL, Log, TEXT("Wrote LLM csv line %d"), WriteCount);
	}

	Archive->Flush();
}

/*
 * convert a Tag to a string. If the Tag is actually a Stat then extract the name of the stat.
*/
FString FLLMCsvWriter::GetTagName(int64 Tag, FLLMPlatformTag* PlatformTags)
{
	if (Tag > (int64)ELLMTag::PlatformTagEnd)
	{
		FString Name = TagToFName(Tag).ToString();

		// if it has a trible slash assume it is a Stat string and extract the descriptive name
		int32 StartIndex = Name.Find(TEXT("///"));
		if (StartIndex != -1)
		{
			StartIndex += 3;
			int32 EndIndex = Name.Find(TEXT("///"), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
			if (EndIndex != -1)
			{
				Name = Name.Mid(StartIndex, EndIndex - StartIndex);
			}
		}

		return Name;
	}
	else if (Tag >= (int32)ELLMTag::PlatformTagStart && Tag <= (int32)ELLMTag::PlatformTagEnd)
	{
		return PlatformTags[Tag - (int32)ELLMTag::PlatformTagStart].Name;
	}
	else
	{
		check(Tag >= 0 && Tag < sizeof(ELLMTagNames) / sizeof(FLLMTagInfo));
		return ELLMTagNames[Tag].Name;
	}
}

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

