// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "ScopeLock.h"


// this is currently incompatible with PLATFORM_USES_FIXED_GMalloc_CLASS, because this ends up being included way too early
// and currently, PLATFORM_USES_FIXED_GMalloc_CLASS is only used in Test/Shipping builds, where we don't have STATS anyway,
// but we can't #include "Stats.h" to find out
#if PLATFORM_USES_FIXED_GMalloc_CLASS
	#define ENABLE_LOW_LEVEL_MEM_TRACKER 0
	#define DECLARE_LLM_MEMORY_STAT(CounterName,StatId,GroupId)
	#define DECLARE_LLM_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API)
#else
	#include "Stats/Stats.h"

	#define LLM_SUPPORTED_PLATFORM (PLATFORM_XBOXONE || PLATFORM_PS4 || PLATFORM_WINDOWS)

	// *** enable/disable LLM here ***
	#define ENABLE_LOW_LEVEL_MEM_TRACKER (!UE_BUILD_SHIPPING && !UE_BUILD_TEST && LLM_SUPPORTED_PLATFORM && WITH_ENGINE && 1)

	// using asset tagging requires a significantly higher number of per-thread tags, so make it optional
	// even if this is on, we still need to run with -llmtagset=assets because of the shear number of stat ids it makes
	#define LLM_ALLOW_ASSETS_TAGS 0

	#define LLM_STAT_TAGS_ENABLED (LLM_ALLOW_ASSETS_TAGS || 0)

	// this controls if the commandline is used to enable tracking, or to disable it. If LLM_COMMANDLINE_ENABLES_FUNCTIONALITY is true, 
	// then tracking will only happen through Engine::Init(), at which point it will be disabled unless the commandline tells 
	// it to keep going (with -llm). If LLM_COMMANDLINE_ENABLES_FUNCTIONALITY is false, then tracking will be on unless the commandline
	// disables it (with -nollm)
	#define LLM_COMMANDLINE_ENABLES_FUNCTIONALITY 1

	#if STATS
		#define DECLARE_LLM_MEMORY_STAT(CounterName,StatId,GroupId) \
			DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_PhysicalLLM); \
			static DEFINE_STAT(StatId)
		#define DECLARE_LLM_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API) \
			DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_PhysicalLLM); \
			extern API DEFINE_STAT(StatId);
	#else
		#define DECLARE_LLM_MEMORY_STAT(CounterName,StatId,GroupId)
		#define DECLARE_LLM_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API)
	#endif
#endif	// #if PLATFORM_USES_FIXED_GMalloc_CLASS


#if STATS
	DECLARE_STATS_GROUP(TEXT("LLM FULL"), STATGROUP_LLMFULL, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Platform"), STATGROUP_LLMPlatform, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Summary"), STATGROUP_LLM, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Overhead"), STATGROUP_LLMOverhead, STATCAT_Advanced);
	DECLARE_STATS_GROUP(TEXT("LLM Assets"), STATGROUP_LLMAssets, STATCAT_Advanced);

	DECLARE_LLM_MEMORY_STAT_EXTERN(TEXT("Engine"), STAT_EngineSummaryLLM, STATGROUP_LLM, CORE_API);
#endif


#if ENABLE_LOW_LEVEL_MEM_TRACKER


#if LLM_STAT_TAGS_ENABLED
	#define LLM_TAG_TYPE uint64
#else
	#define LLM_TAG_TYPE uint8
#endif

// estimate the maximum amount of memory LLM will need to run on a game with around 4 million allocations.
// Make sure that you have debug memory enabled on consoles (on screen warning will show if you don't)
// (currently only used on PS4 to stop it reserving a large chunk up front. This will go away with the new memory system)
#define LLM_MEMORY_OVERHEAD (600LL*1024*1024)

/*
 * LLM Trackers
 */
enum class ELLMTracker : uint8
{
	Platform,
	Default,

	Max,	// see FLowLevelMemTracker::UpdateStatsPerFrame when adding!
};

/*
 * optional tags that need to be enabled with -llmtagsets=x,y,z on the commandline
 */
enum class ELLMTagSet : uint8
{
	None,
	Assets,
	AssetClasses,
	
	Max,	// note: check out FLowLevelMemTracker::ShouldReduceThreads and IsAssetTagForAssets if you add any asset-style tagsets
};

/*
 * Enum values to be passed in to LLM_SCOPE() macro
 * *** This enum must be kept in sync with ELLMTagNames ***
 */
enum class ELLMTag : LLM_TAG_TYPE
{
	Untagged = 0,
	Paused,					// special tag that indicates the thread is paused, and that the tracking code should ignore the alloc

	TrackedTotal,
	UntrackedTotal,
	PlatformTrackedTotal,
	PlatformUntrackedTotal,
	SmallBinnedAllocation,
	LargeBinnedAllocation,
	ThreadStack,
	ProgramSizePlatform,
	ProgramSize,
	BackupOOMMemoryPoolPlatform,
	BackupOOMMemoryPool,
	GenericPlatformMallocCrash,
	GenericPlatformMallocCrashPlatform,
	EngineMisc,
	TaskGraphTasksMisc,
	Audio,
	FName,
	Networking,
	Meshes,
	Stats,
	Shaders,
	Textures,
	RenderTargets,
	RHIMisc,
	PhysXTriMesh,
	PhysXConvexMesh,
	AsyncLoading,
	UObject,
	Animation,
	StaticMesh,
	Materials,
	Particles,
	GC,
	UI,
	PhysX,
	EnginePreInitMemory,
	EngineInitMemory,
	RenderingThreadMemory,
	LoadMapMisc,
	StreamingManager,
	GraphicsPlatform,
	FileSystem,

	GenericTagCount,

	//------------------------------
	// Platform tags
	PlatformTagStart = 100,
	PlatformRHITagStart = 200,
	PlatformTagEnd = 0xff,

	// anything above this value is treated as an FName for a stat section
};

/*
 * LLM utility macros
 */
#define LLM(x) x
#define SCOPE_NAME PREPROCESSOR_JOIN(LLMScope,__LINE__)

///////////////////////////////////////////////////////////////////////////////////////
// These are the main macros to use externally when tracking memory
///////////////////////////////////////////////////////////////////////////////////////

/*
 * LLM scope macros
 */
#define LLM_SCOPE(Tag) FLLMScopedTag SCOPE_NAME(Tag, ELLMTagSet::None, ELLMTracker::Default);
#define LLM_PLATFORM_SCOPE(Tag) FLLMScopedTag SCOPE_NAME(Tag, ELLMTagSet::None, ELLMTracker::Platform);

 /*
 * LLM Pause scope macros
 */
#define LLM_SCOPED_PAUSE_TRACKING() FLLMScopedPauseTrackingWithAmountToTrack SCOPE_NAME(NAME_None, 0, ELLMTracker::Max);
#define LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(Tracker) FLLMScopedPauseTrackingWithAmountToTrack SCOPE_NAME(NAME_None, 0, Tracker);
#define LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(Tag, Amount, Tracker) FLLMScopedPauseTrackingWithAmountToTrack SCOPE_NAME(Tag, Amount, Tracker);

/*
 * LLM Stat scope macros (if stats system is enabled)
 */
#if LLM_STAT_TAGS_ENABLED
	#define LLM_SCOPED_TAG_WITH_STAT(Stat, Tracker) FLLMScopedTag SCOPE_NAME(GET_STATFNAME(Stat), ELLMTagSet::None, Tracker);
	#define LLM_SCOPED_TAG_WITH_STAT_IN_SET(Stat, Set, Tracker) FLLMScopedTag SCOPE_NAME(GET_STATFNAME(Stat), Set, Tracker);
	#define LLM_SCOPED_TAG_WITH_STAT_NAME(StatName, Tracker) FLLMScopedTag SCOPE_NAME(StatName, ELLMTagSet::None, Tracker);
	#define LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(StatName, Set, Tracker) FLLMScopedTag SCOPE_NAME(StatName, Set, Tracker);
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG(Stat) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMPlatform); LLM_SCOPED_TAG_WITH_STAT(Stat, ELLMTracker::Platform);
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG_IN_SET(Stat, Set) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMPlatform); LLM_SCOPED_TAG_WITH_STAT_IN_SET(Stat, Set, ELLMTracker::Platform);
	#define LLM_SCOPED_SINGLE_STAT_TAG(Stat) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMFULL); LLM_SCOPED_TAG_WITH_STAT(Stat, ELLMTracker::Default);
	#define LLM_SCOPED_SINGLE_STAT_TAG_IN_SET(Stat, Set) DECLARE_LLM_MEMORY_STAT(TEXT(#Stat), Stat, STATGROUP_LLMFULL); LLM_SCOPED_TAG_WITH_STAT_IN_SET(Stat, Set, ELLMTracker::Default);
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_STAT_AND_AMOUNT(Stat, Amount, Tracker) FLLMScopedPauseTrackingWithAmountToTrack SCOPE_NAME(GET_STATFNAME(Stat), Amount, Tracker);
	#define LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(Object, Set) LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(FLowLevelMemTracker::Get().IsTagSetActive(Set) ? (FDynamicStats::CreateMemoryStatId<FStatGroup_STATGROUP_LLMAssets>(FName(*(Object)->GetFullName())).GetName()) : NAME_None, Set, ELLMTracker::Default);

	// special stat pushing for when asset tracking is on, which abuses the poor thread tracking
	#define LLM_PUSH_STATS_FOR_ASSET_TAGS() if (FLowLevelMemTracker::Get().IsTagSetActive(ELLMTagSet::Assets)) FLowLevelMemTracker::Get().UpdateStatsPerFrame();
#else
	#define LLM_SCOPED_TAG_WITH_STAT(...)
	#define LLM_SCOPED_TAG_WITH_STAT_IN_SET(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_STAT_AND_AMOUNT(...)
	#define LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(...)
	#define LLM_PUSH_STATS_FOR_ASSET_TAGS()
#endif

typedef void*(*LLMAllocFunction)(size_t);
typedef void(*LLMFreeFunction)(void*, size_t);

class FLLMAllocator
{
public:
	FLLMAllocator()
	: PlatformAlloc(NULL)
	, PlatformFree(NULL)
	, Alignment(0)
	{
	}

	void Initialise(LLMAllocFunction InAlloc, LLMFreeFunction InFree, int32 InAlignment)
	{
		PlatformAlloc = InAlloc;
		PlatformFree = InFree;
		Alignment = InAlignment;
	}

	void* Alloc(size_t Size)
	{
		Size = Align(Size, Alignment);
		FScopeLock Lock(&CriticalSection);
		void* Ptr = PlatformAlloc(Size);
		Total += Size;
		check(Ptr);
		return Ptr;
	}

	void Free(void* Ptr, size_t Size)
	{
		Size = Align(Size, Alignment);
		FScopeLock Lock(&CriticalSection);
		PlatformFree(Ptr, Size);
		Total -= Size;
	}

	int64 GetTotal() const
	{
		FScopeLock Lock((FCriticalSection*)&CriticalSection);
		return Total;
	}

private:
	FCriticalSection CriticalSection;
	LLMAllocFunction PlatformAlloc;
	LLMFreeFunction PlatformFree;
	int64 Total;
	int32 Alignment;
};

struct FLLMPlatformTag
{
	int32 Tag;
	const TCHAR* Name;
	FName StatName;
	FName SummaryStatName;
};

/*
 * The main LLM tracker class
 */
class CORE_API FLowLevelMemTracker
{
public:

	// get the singleton, which makes sure that we always have a valid object
	static FLowLevelMemTracker& Get();

	bool IsEnabled();

	// we always start up running, but if the commandline disables us, we will do it later after main
	// (can't get the commandline early enough in a cross-platform way)
	void ProcessCommandLine(const TCHAR* CmdLine);

	// this is the main entry point for the class - used to track any pointer that was allocated or freed 
	void OnLowLevelAlloc(ELLMTracker Tracker, const void* Ptr, uint64 Size, ELLMTag DefaultTag = ELLMTag::Untagged);		// DefaultTag is used it no other tag is set
	void OnLowLevelFree(ELLMTracker Tracker, const void* Ptr, uint64 CheckSize);

	// call if an allocation is moved in memory, such as in a defragger
	void OnLowLevelAllocMoved(ELLMTracker Tracker, const void* Dest, const void* Source);

	// expected to be called once a frame, from game thread or similar - updates memory stats 
	void UpdateStatsPerFrame(const TCHAR* LogName=nullptr);

	// Optionally set the amount of memory taken up before the game starts for executable and data segments
	void InitialiseProgramSize();
	void SetProgramSize(uint64 InProgramSize);

	// console command handler
	bool Exec(const TCHAR* Cmd, FOutputDevice& Ar);

	// are we in the more intensive asset tracking mode, and is it active
	bool IsTagSetActive(ELLMTagSet Set);

	// for some tag sets, it's really useful to reduce threads, to attribute allocations to assets, for instance
	bool ShouldReduceThreads();

	void RegisterPlatformTag(int32 Tag, const TCHAR* Name, FName StatName, FName SummaryStatName);

private:
	FLowLevelMemTracker();

	~FLowLevelMemTracker();

	void InitialiseTrackers();

	class FLLMTracker* GetTracker(ELLMTracker Tracker);

	friend class FLLMScopedTag;
	friend class FLLMScopedPauseTrackingWithAmountToTrack;

	FLLMAllocator Allocator;
	
	bool bFirstTimeUpdating;

	uint64 ProgramSize;

	bool bIsDisabled;

	bool ActiveSets[(int32)ELLMTagSet::Max];

	bool bCanEnable;

	bool bCsvWriterEnabled;

	bool bInitialisedTrackers;

	FLLMPlatformTag PlatformTags[(int32)ELLMTag::PlatformTagEnd + 1 - (int32)ELLMTag::PlatformTagStart];

	FLLMTracker* Trackers[(int32)ELLMTracker::Max];
};

/*
 * LLM scope for tracking memory
 */
class CORE_API FLLMScopedTag
{
public:
	FLLMScopedTag(FName StatIDName, ELLMTagSet Set, ELLMTracker Tracker);
	FLLMScopedTag(ELLMTag Tag, ELLMTagSet Set, ELLMTracker Tracker);
	~FLLMScopedTag();
protected:
	void Init(int64 Tag, ELLMTagSet Set, ELLMTracker Tracker);
	ELLMTagSet TagSet;
	ELLMTracker TrackerSet;
	bool Enabled;
};

/*
* LLM scope for pausing LLM (disables the allocation hooks)
*/
class CORE_API FLLMScopedPauseTrackingWithAmountToTrack
{
public:
	FLLMScopedPauseTrackingWithAmountToTrack(FName StatIDName, int64 Amount, ELLMTracker TrackerToPause);
	FLLMScopedPauseTrackingWithAmountToTrack(ELLMTag Tag, int64 Amount, ELLMTracker TrackerToPause);
	~FLLMScopedPauseTrackingWithAmountToTrack();
protected:
	void Init(int64 Tag, int64 Amount, ELLMTracker TrackerToPause);
};

#else
	#define LLM(...)
	#define LLM_SCOPE(...)
	#define LLM_PLATFORM_SCOPE(...)
	#define LLM_SCOPED_TAG_WITH_STAT(...)
	#define LLM_SCOPED_TAG_WITH_STAT_IN_SET(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME(...)
	#define LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_PLATFORM_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_SINGLE_RHI_STAT_TAG(...)
	#define LLM_SCOPED_SINGLE_RHI_STAT_TAG_IN_SET(...)
	#define LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(...)
	#define LLM_SCOPED_PAUSE_TRACKING()
	#define LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(...)
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(...)
	#define LLM_SCOPED_PAUSE_TRACKING_WITH_STAT_AND_AMOUNT(...)
	#define LLM_PUSH_STATS_FOR_ASSET_TAGS()
#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

