// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

/** Set to zero to disable the slate stats system completely, meaning all stats-related structures and code are compiled out. */
//#define SLATE_STATS (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)

#ifndef WITH_SLATE_STATS
	#define WITH_SLATE_STATS 0
#endif

#define SLATE_STATS (WITH_SLATE_STATS | WITH_EDITOR)

/** 
 * Predefined detail levels to make choosing level slightly easier.
 * The _OFF setting effectively turns off any stat while still keeping the stats system compiled in. 
 */
#define SLATE_STATS_DETAIL_LEVEL_FULL INT_MAX
#define SLATE_STATS_DETAIL_LEVEL_HI INT_MAX/4*3
#define SLATE_STATS_DETAIL_LEVEL_MED INT_MAX/4*2
#define SLATE_STATS_DETAIL_LEVEL_LOW INT_MAX/4*1
#define SLATE_STATS_DETAIL_LEVEL_OFF INT_MIN+1

/** Controls what level of detail of stats should be compiled in. Detail level is a simple int. Any stat with a detail level less than this value will be compiled in. */
#define SLATE_STATS_DETAIL_LEVEL SLATE_STATS_DETAIL_LEVEL_MED

#if SLATE_STATS

/** 
 * Slate Stats System Motivation
 * ---------------------------
 * Slate uses it's own stats system to gather performance metrics. There are a few reasons it does not use the builtin UE4 stats system:
 *   1. UE4 stats are threadsafe, thus heavier weight. Slate is single-threaded by nature due to UI constraints.
 *   2. Slate stats can optionally carry additional context (like class or debug info) used to provide more info when digesting the profile.
 *   3. Using a lighter weight system allows us to add more tracking by default.
 *   4. Slate uses a custom hierarchical system that allows it to spit out a CSV containing a complete hierarchical inclusive/exclusive profile.
 * 
 * Slate Stats System Overview
 * ---------------------------
 * You need to declare your stat globally somewhere. It should generally be a static variable:
 * 
 *   SLATE_DECLARE_CYCLE_COUNTER(GSlateOnPaint, "OnPaint");
 * 
 * Next you go the scope of code to provide and add a scope counter (the types will be explained below).
 * 
 *   SLATE_CYCLE_COUNTER_SCOPE(GSlateOnPaint);
 * 
 * This enables the Slate Stats system to track this scope of code and associate it with the "OnPaint" stat.
 * If you want to track the OnPaint call of each type of widget separately, you would use the _CUSTOM version of the macro and provide it with
 * an FName to track along with the stat like so (assuming "this" is an SWidget):
 * 
 *   SLATE_CYCLE_COUNTER_SCOPE_CUSTOM(GSlateOnPaint, GetType());
 * 
 * However, note that custom info is only available when doing a full hierarchical capture (as explained below).
 * 
 * Slate supports two modes of stats - flat and hierarchical. Stats are hierarchical by default. Use the _FLAT form of the macro 
 * to declare a stat that is only used for flattened tracking. This is faster when you don't need this stat for a full hierarchical profile.
 *
 *   Flat
 *   ----
 *   Flat view is a standard inclusive sum of cycles. Each scope simply records a start and end time and adds it to the global stat accumulator.
 *   This happens until the system is told to display the values, at which point it averages the cycle accumulators over the number of frames since
 *   the last summary. The resultant view is essentially a list of all declared stats along with the average inclusive time spent in that stat's scope. 
 *   
 *   The system handles re-entrant stats. So if you are processing a hierarchy (as Slate widgets essentially are), re-entering the same stat will do
 *   nothing until the last scope is exited.
 *   
 *   Hierarchical
 *   ------------
 *   Hierarchical capture essentially creates a complete hierarchical profile of a frame. Since it has to track EACH stat entry that frame, it can
 *   have a non-trivial performance and memory impact on the application. However, the result is similar to the data from a fully instrumented profile
 *   of your application, with a few notable details:
 *     1. You are instrumenting sections of code, not functions. This allows for finer-grained classification of your code.
 *     2. each entry also carries any context you passed it at runtime, allowing for even better context than an instrumented profiler can give you.
 *     3. Stats are not subject to the whims of inlining, etc, since they are injected at compile-time.
 *   While Slate goes out of its way to make this capture as cheap as possible, there is a lot of data being gathered each frame. Therefore, Slate does NOT
 *   do a hierarchical capture continually. Instead, the stats system must be triggered (programmatically) to enable hierarchical capture right before a 
 *   frame is started. This is done when calling EndFrame() on the FSlateStatHierarchy singleton:
 *   
 *     FSlateStatHierarchy::Get().EndFrame(true|false);
 *     
 *   This can be triggered by end users through the Slate.Stats.Hierarchy.Trigger cvar.
 *   
 *   Since there is no guarantee that the stat call stack will be equivalent from one frame to the next, we can only accumulate ONE frame.
 *   
 *   When a hierarchical capture frame ends, Slate does some additional work to gather up all instances, derive "full paths" to each entry, and derive
 *   the exclusive time for that instance. Again, Slate tries to do this as fast as possible, but there can be 100,000s of entries in a frame, so that
 *   can only be as fast as those entries can be traversed. When finished, Slate spits out a CSV into the Saved/ folder containing the following fields:
 *     1. Path - full path to the entry. 
 *     2. Stat Description
 *     3. Stat additional context (Custom info)
 *     4. Inclusive time
 *     5. Exclusive time
 *     
 *   Path is essentially a pathway through the hierarchy of samples to the entry (a stat call-stack if you will). This allows analysis tools to understand 
 *   the capture hierarchy so they can do things like restrict to subtrees of the capture, create caller-callee views, etc. Essentially typical instrumented 
 *   profiler things.
 * 
 * FSlateStatCycleCounter - Global storage of a cycle stat. This should be declared as a global using the SLATE_DECLARE_CYCLE_COUNTER macro.
 * Counters don't update themselves, they rely on the scope classes to update their values.
 * All the methods of this class are essentially internal and there is no need for an end-user to call them directly.
 */
class FSlateStatCycleCounter
{
public:
	/** 
	 * Default ctor. Registers itself with the global list of counters (so the system can iterate over them).
	 * @param InCounterName Name of the counter. Essentially an arbitrary description string, but try to keep it terse.
	 */
	SLATECORE_API FSlateStatCycleCounter(FName InCounterName);
	
	/**
	 * Unregisteres the counter from the global list. Uses a global "memory pool" to avoid initialization/shutdown dependencies.
	 */
	SLATECORE_API ~FSlateStatCycleCounter();

	/** Public access to report on a stat. Time is pre-converted to milliseconds. */
	double GetLastComputedAverageInclusiveTime() const
	{
		return LastComputedAverageInclusiveTime;
	}

	/** Public access to report on a stat. */
	FName GetName() const
	{
		return Name;
	}

	/** Access to the global list of counters to iterate over. Internally uses a memory pool (not a static instance) to avoid shutdown early-destruction issues. */
	static SLATECORE_API const TArray<FSlateStatCycleCounter*>& GetRegisteredCounters();

	/** Perform all end-frame work. Slate makes this call automatically for you, so end users should not have to call it. */
	static SLATECORE_API void EndFrame(double CurrentTime);

	static SLATECORE_API bool AverageInclusiveTimesWereUpdatedThisFrame();

private:
	/** Needs to be called after the stats have been reported. This resets the accumulated time. */
	SLATECORE_API void Reset();

	/** 
	 * A global list of counters that all instances register with. Allows easier iteration. 
	 * Internally creates a dynamic allocation and "leaks" it (aka: memory pool) so it doesn't get destroyed in the wrong order at shutdown.
	 * Declared out of line so we can have a single instance across module boundaries.
	 */
	static SLATECORE_API TArray<FSlateStatCycleCounter*>& GetGlobalRegisteredCounters();

	// let these classes mutate the counter values.
	template<bool> friend class FSlateStatCycleCounterScopeFlat;
	template<bool> friend class FSlateStatCycleCounterScopeHierarchical;

	/** Display name of the stat. */
	FName Name;
	/** Accumulated time since the last report. Something needs to track frames since last report for this to be useful. */
	double InclusiveTime;
	/** Last averaged stat computed by the stats system. This is the value that can be used for display purposes. */
	double LastComputedAverageInclusiveTime;

	/** 
	 * Used by the scope classes. 
	 * This really makes the system single-threaded only because we can only track one instance of a counter at a time like this.
	 * Saves us from having to look it up.
	 * 
	 * When a timer is re-entrant, we don't want to keep starting the time over. So we only track the outer level. 
	 */
	int32 StackDepth;
	/** 
	 * Used by the scope classes. 
	 * This really makes the system single-threaded only because we can only track one instance of a counter at a time like this.
	 * Saves us from having to look it up.
	 * 
	 * Tracks when we started timing this counter so we can compute a delta.
	 */
	double StartTime;
};

/** 
 * Scoped tracking of a FSlateStatCycleCounter. 
 * Super-lean, starts timing in the ctor, and stops in the dtor. 
 * Only tracks time when it is at the top level, effectively ignoring re-entrancy. 
 * 
 * Inlined purposefully to keep the call overhead down.
 */
template <bool bShouldBeCompiledIn>
class FSlateStatCycleCounterScopeFlat
{
public:
	/** Ctor connects this scope to a specific counter to accumulate to and starts the timing. */
	FSlateStatCycleCounterScopeFlat( FSlateStatCycleCounter& InCounter )
		: Counter(InCounter)
	{
		extern SLATECORE_API int32 GSlateStatsFlatEnable;
		if (GSlateStatsFlatEnable > 0)
		{
			StartTiming();
		}
	}
	/** Dtor stops the timing and accumulates. */
	~FSlateStatCycleCounterScopeFlat()
	{
		extern SLATECORE_API int32 GSlateStatsFlatEnable;
		if (GSlateStatsFlatEnable > 0)
		{
			StopTiming();
		}
	}

private:
	/** Starts the timing. Ignores the call if we detect re-entrancy. */
	void StartTiming()
	{
		// only start timing when the stack depth is zero.
		if (Counter.StackDepth++ == 0)
		{
			Counter.StartTime = FPlatformTime::Seconds();
		}
	}

	/** Stops the timing. Ignores the call if we detect re-entrancy. */
	void StopTiming()
	{
		// only stop timing when the stack depth goes back to zero, meaning we're finally out of our re-entrant stack.
		if (--Counter.StackDepth == 0)
		{
			const double Delta = FPlatformTime::Seconds() - Counter.StartTime;
			Counter.InclusiveTime += Delta;
		}
	}

	// stores a reference to the stat we will be accumulating to.
	FSlateStatCycleCounter& Counter;
};

/** Class only exists so stats that don't meet the defined SLATE_STATS_DETAIL_LEVEL are not compiled in. */
template <>
class FSlateStatCycleCounterScopeFlat<false>
{
public:
	FSlateStatCycleCounterScopeFlat(FSlateStatCycleCounter&) {}
};



/** Tracks info for a hierarchical stats instance (explained in FSlateStatHierarchy). */
struct FSlateStatHierarchyEntry
{
	/** Initializes all the data needed for a hierarchical entry. */
	FSlateStatHierarchyEntry(FName InCounterName, FName InCustomName, double StartTime, int32 InStackDepth) : CounterName(InCounterName), CustomName(InCustomName), InclusiveTime(StartTime), ExclusiveTime(0.0), StackDepth(InStackDepth) {}
	/** Counter display name. Same as flattened stats. */
	FName CounterName;
	/** Custom context string. Used for things like printing RTTI when tracking a virtual function hierarchically. */
	FName CustomName;
	/** Time measured for this stat. */
	double InclusiveTime;
	/** Exclusive time measured for this stat. Not computed immediately. Has to be calculated when the frame is over. */
	double ExclusiveTime;
	/** Depth of this stat in the stat hierarchy. Will be used during reporting to unwind the stack and compute a full path for each entry in the list. */
	int32 StackDepth;
};

/** 
 * Hierarchical tracking of stats.
 * Heavier weight than flattened tracking done by FSlateStatCycleCounter because it has to track EACH scope separately in a dynamic array.
 * Due to this, we don't track this every frame, and only show the results of a single frame capture that we decide in advance to track
 * (ie, we can't do something like hitch detection, which is only disovered in post).
 * 
 * Still fairly lean as far as hierarchical tracking goes, but does a lot more timing calls in highly re-entrant stats
 * (where the flattened system only tracks the top level).
 * 
 * Like flattened stats, all of this only works correctly when stats are gathered on a single thread (the same thread that reports them)!
 */
class FSlateStatHierarchy
{
public:
	/** Class is a singleton, so we reserve some space for the hierarchy when it is first constructed. */
	FSlateStatHierarchy();

	/** 
	 * Singleton access. 
	 * Declared out of line so we can have a single instance across module boundaries.
	 */
	static SLATECORE_API FSlateStatHierarchy& Get();

	/** 
	 * Recursive function to traverse the entries at the end of a frame and compute the exclusive times as quickly as possible.
	 * The basic premise is to increment of each entry (Index param), while tracking the index of the immediate parent of that 
	 * entry.
	 *   * If we are one level deeper than our parent, then all the inclusive time used by this entry is NOT part of the 
	 * exclusive time of the parent, so we subtract it off.
	 *   * When we descend more than one level deeper than the parent, we know that the preceding entry is the parent, as all
	 *     entries are added in order (and single-threaded).
	 *   * As we ascend from our recursive stack we update the index so the calling code knows to skip over the descendents we
	 *     already processed.
	 *     
	 * This results in an extremely quick processing of the entries, mostly bound by our stack depth.
	 * 
	 * @param Index - index of the entry we are processing 
	 * @param ParentIndex - index of this entry's parent. May have ot be updated if we discover this entry has descended a level in the stack.
	 * @return the Index AFTER the last one we processed.
	 */
	int32 ComputeExclusiveTimes(int32 Index = 1, int32 ParentIndex = 0)
	{
		const int32 NumEntries = StatEntries.Num();
		// keep going until we reach the end.
		while (Index < NumEntries)
		{
			// grab the info about our parent.
			FSlateStatHierarchyEntry& ParentEntry = StatEntries[ParentIndex];
			const int32 ParentDepth = ParentEntry.StackDepth;
			// look at our current depth.
			const int32 Depth = StatEntries[Index].StackDepth;
			// we are immediately below our parent. Subtract off the entry's inclusive time
			// because it isn't part of the parent's exclusive time.
			if (Depth == ParentDepth + 1)
			{
				const double InclusiveTime = StatEntries[Index].InclusiveTime;
				ParentEntry.ExclusiveTime -= InclusiveTime;
				++Index;
			}
			// we have descended one level in the stack. Update the parent and go again.
			else if (Depth == ParentDepth + 2)
			{
				Index = ComputeExclusiveTimes(Index, Index - 1);
			}
			// There may be many roots, so we just have to treat all level 0's as siblings.
			else if (Depth == 0 && ParentDepth == 0)
			{
				ParentIndex = Index;
				++Index;
			}
			// we are ascending out of the stack, so just return.
			else
			{
				return Index;
			}
		
		}
		// we've reached the end of our list of entries.
		return Index;
	}

	/** 
	 * Needs to be called every time a frame ends. 
	 * Empties the stack of stats (without reclaiming the memory). 
	 * Set bTrackNextFrame to true to actually enable hierarchical tracking on the next frame. 
	 */
	void EndFrame(bool bTrackNextFrame);

	/** tracks whether we are actually gathering hierarchical stats this frame. */
	bool IsTrackingThisFrame() const
	{
		return bTrackThisFrame;
	}

	/** Access to the hierarhcy for reporting. */
	const TArray<FSlateStatHierarchyEntry>& GetStatEntries() const { return StatEntries; }

	/** Interface to start tracking an instance. Returns the tracking instance so we don't have to look it up later.. */
	FSlateStatHierarchyEntry* StartStat(FName CounterName, FName CustomName, double StartTime)
	{
		if (!bTrackThisFrame) return NULL;
		checkfSlow(StatEntries.Num() >= StatEntries.Max(), TEXT("You have overrun the Slate Stats hierarhical profiling entries limit. Profile a simpler scene or adjust the #define SLATE_STATS_HIERARCHY_MAX_ENTRIES to a larger value and recompile your code."));
		StatEntries.Emplace(CounterName, CustomName, StartTime, StackDepth++);
		return &StatEntries.Last();
	}

	/** Called when done tracking an instance. Pass the entry returned by StartStat(). */
	void StopStat(FSlateStatHierarchyEntry* Entry, double EndTime)
	{
		if (!Entry) return;
		Entry->InclusiveTime = EndTime - Entry->InclusiveTime;
		// This isn't the real exclusive time yet. At the end of the frame we will subtract off child times.
		Entry->ExclusiveTime = Entry->InclusiveTime;
		// indicate that we are ascending our stack of entries.
		--StackDepth;
	}

private:
	/** 
	 * Stat Entries measured this frame. Usually empty unless EndFrame() is called with bTrackNextFrame = true. 
	 * 
	 * WARNING: This array SHOULD NOT be resized in a frame. If it is, the system will assert or crash in non-checkslow builds.
	 * If this happens, adjust SLATE_STATS_HIERARCHY_MAX_ENTRIES for your needs.
	 */
	TArray<FSlateStatHierarchyEntry> StatEntries;
	/** Current stat hierarchy depth. */
	int32 StackDepth;
	/** If false, does not actually track the hierarchy that frame. */
	bool bTrackThisFrame;

};

/** 
 * Tracks a flattened stat, but also adds to the hierarchical tracker. 
 * Supports a custom FName to be passed in to track some context specific info about this stat in the hierarchy (like a specific class name when tracking a virtual method).
 * This is a completely separate class to keep the flat version super lean. 
 * 
 * Inlined purposefully to keep the call overhead down.
 */
template <bool bShouldBeCompiledIn>
class FSlateStatCycleCounterScopeHierarchical
{
public:
	/** Ctor mimics flat stats logic, but also allocates a hierarchical entry if we are capturing hierarchical stats this frame. */
	FSlateStatCycleCounterScopeHierarchical( FSlateStatCycleCounter& InCounter )
		: Counter(InCounter)
		, HierarchyEntry(nullptr)
	{
		StartTiming(NAME_None);
	}
	/**
	 * Ctor mimics flat stats logic, but also allocates a hierarchical entry if we are capturing hierarchical stats this frame. 
	 * This version allows passing a custom FName to associate with the entry.
	 */
	FSlateStatCycleCounterScopeHierarchical( FSlateStatCycleCounter& InCounter, FName CustomName )
		: Counter(InCounter)
		, HierarchyEntry(nullptr)
	{
		StartTiming(CustomName);
	}
	/** Dtor mimics flat stats logic, but also updates the hierarchical entry if we are capturing hierarchical stats this frame. */
	~FSlateStatCycleCounterScopeHierarchical()
	{
		StopTiming();
	}

private:
	/**
	 * Starts the hierarchical timing. If we are not tracking this frame, falls back to simple flat tracking.
	 * The main difference is that re-entrant stats entries ARE tracked individually by the hierarchical tracker.
	 */
	void StartTiming(FName CustomName)
	{
		// Skip a bunch of work if we are not tracking the hierarchy this frame, essentially falling back to the flat stat logic.
		if (FSlateStatHierarchy::Get().IsTrackingThisFrame())
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (Counter.StackDepth++ == 0)
			{
				Counter.StartTime = CurrentTime;
			}
			HierarchyEntry = FSlateStatHierarchy::Get().StartStat(Counter.Name, CustomName, CurrentTime);
		}
		else
		{
			extern SLATECORE_API int32 GSlateStatsFlatEnable;
			if (GSlateStatsFlatEnable > 0)
			{
				if (Counter.StackDepth++ == 0)
				{
					Counter.StartTime = FPlatformTime::Seconds();
				}
			}
		}
	}

	/**
	 * Stops the hierarchical timing. If we are not tacking this frame, falls back to simple flat tracking.
	 */
	void StopTiming()
	{
		// Skip a bunch of work if we are not tracking the hierarchy this frame, essentially falling back to the flat stat logic.
		if (HierarchyEntry != NULL)
		{
			const double CurrentTime = FPlatformTime::Seconds();
			FSlateStatHierarchy::Get().StopStat(HierarchyEntry, CurrentTime);
			// only increment the stat when the stack depth goes back to zero, meaning we're finally out of our re-entrant stack.
			if (--Counter.StackDepth == 0)
			{
				const double Delta = CurrentTime - Counter.StartTime;
				Counter.InclusiveTime += Delta;
			}
		}
		else
		{
			extern SLATECORE_API int32 GSlateStatsFlatEnable;
			if (GSlateStatsFlatEnable > 0)
			{
				// only increment the stat when the stack depth goes back to zero, meaning we're finally out of our re-entrant stack.
				if (--Counter.StackDepth == 0)
				{
					const double Delta = FPlatformTime::Seconds() - Counter.StartTime;
					Counter.InclusiveTime += Delta;
				}
			}
		}
	}

	/** The counter we are tracking for. */
	FSlateStatCycleCounter& Counter;
	/** The actual hierarchical entry we are recording to. This stores all the real data. */
	FSlateStatHierarchyEntry* HierarchyEntry;
};

/** Class only exists so stats that don't meet the defined SLATE_STATS_DETAIL_LEVEL are not compiled in. */
template <>
class FSlateStatCycleCounterScopeHierarchical < false >
{
public:
	FSlateStatCycleCounterScopeHierarchical(FSlateStatCycleCounter&) {}
	FSlateStatCycleCounterScopeHierarchical(FSlateStatCycleCounter&, FName) {}
};

/** Declares a Slate Stat cycle counter. */
#define SLATE_DECLARE_CYCLE_COUNTER(CounterName, CounterDescription) \
	FSlateStatCycleCounter CounterName(CounterDescription)

/** Track a Slate Stat, also adding it to the hierarchy if we are capturing hierarchical stats this frame. */
#define SLATE_CYCLE_COUNTER_SCOPE(StatCounterWes) \
	FSlateStatCycleCounterScopeHierarchical<(SLATE_STATS_DETAIL_LEVEL != SLATE_STATS_DETAIL_LEVEL_OFF)> PREPROCESSOR_JOIN(TrackStat,__LINE__)(StatCounterWes)

/** Track a Slate Stat, also adding it to the hierarchy if we are capturing hierarchical stats this frame. With a given detail level so it can be compiled out. */
#define SLATE_CYCLE_COUNTER_SCOPE_DETAILED(DetailLevel, StatCounterWes) \
	FSlateStatCycleCounterScopeHierarchical<(DetailLevel <= SLATE_STATS_DETAIL_LEVEL) && (DetailLevel != SLATE_STATS_DETAIL_LEVEL_OFF)> PREPROCESSOR_JOIN(TrackStat,__LINE__)(StatCounterWes)

/** Track a Slate Stat, also adding it to the hierarchy if we are capturing hierarchical stats this frame, along with custom context. */
#define SLATE_CYCLE_COUNTER_SCOPE_CUSTOM(StatCounterWes,CustomName) \
	FSlateStatCycleCounterScopeHierarchical<(SLATE_STATS_DETAIL_LEVEL != SLATE_STATS_DETAIL_LEVEL_OFF)> PREPROCESSOR_JOIN(TrackStat,__LINE__)(StatCounterWes, CustomName)

/** Track a Slate Stat, also adding it to the hierarchy if we are capturing hierarchical stats this frame, along with custom context. With a given detail level so it can be compiled out. */
#define SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(DetailLevel, StatCounterWes,CustomName) \
	FSlateStatCycleCounterScopeHierarchical<(DetailLevel <= SLATE_STATS_DETAIL_LEVEL) && (DetailLevel != SLATE_STATS_DETAIL_LEVEL_OFF)> PREPROCESSOR_JOIN(TrackStat,__LINE__)(StatCounterWes, CustomName)

/** Track a Slate Stat, only for flattened display. Slightly cheaper than checking dynamically. It doesn't make sense to have a custom version as flattened stats don't gather custom data right now. */
#define SLATE_CYCLE_COUNTER_SCOPE_FLAT(StatCounterWes) \
	FSlateStatCycleCounterScopeFlat<(SLATE_STATS_DETAIL_LEVEL != SLATE_STATS_DETAIL_LEVEL_OFF)> PREPROCESSOR_JOIN(TrackStat,__LINE__)(StatCounterWes)

/** Track a Slate Stat, only for flattened display. Slightly cheaper than checking dynamically. With a given detail level so it can be compiled out. */
#define SLATE_CYCLE_COUNTER_SCOPE_FLAT_DETAILED(DetailLevel, StatCounterWes) \
	FSlateStatCycleCounterScopeFlat<(DetailLevel <= SLATE_STATS_DETAIL_LEVEL) && (DetailLevel != SLATE_STATS_DETAIL_LEVEL_OFF)> PREPROCESSOR_JOIN(TrackStat,__LINE__)(StatCounterWes)

#define SLATE_STATS_END_FRAME(CurrentTime) FSlateStatCycleCounter::EndFrame(CurrentTime)

#else
#define SLATE_DECLARE_CYCLE_COUNTER(CounterName, CounterDescription)
#define SLATE_CYCLE_COUNTER_SCOPE(StatCounterWes)
#define SLATE_CYCLE_COUNTER_SCOPE_DETAILED(DetailLevel,StatCounterWes)
#define SLATE_CYCLE_COUNTER_SCOPE_CUSTOM(StatCounterWes,CustomName)
#define SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(DetailLevel,StatCounterWes,CustomName)
#define SLATE_CYCLE_COUNTER_SCOPE_FLAT(StatCounterWes)
#define SLATE_CYCLE_COUNTER_SCOPE_FLAT_DETAILED(DetailLevel,StatCounterWes)
#define SLATE_STATS_END_FRAME(CurrentTime)

#endif
