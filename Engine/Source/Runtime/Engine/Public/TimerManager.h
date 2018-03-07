// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TimerManager.h: Global gameplay timer facility
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"

class UGameInstance;

DECLARE_DELEGATE(FTimerDelegate);

/** Simple interface to wrap a timer delegate that can be either native or dynamic. */
struct FTimerUnifiedDelegate
{
	/** Holds the delegate to call. */
	FTimerDelegate FuncDelegate;
	/** Holds the dynamic delegate to call. */
	FTimerDynamicDelegate FuncDynDelegate;
	/** Holds the tfunction callback to call. */
	TFunction<void(void)> FuncCallback;

	FTimerUnifiedDelegate() {};
	FTimerUnifiedDelegate(FTimerDelegate const& D) : FuncDelegate(D) {};
	FTimerUnifiedDelegate(FTimerDynamicDelegate const& D) : FuncDynDelegate(D) {};
	FTimerUnifiedDelegate(TFunction<void(void)>&& Callback) : FuncCallback(MoveTemp(Callback)) {}
	
	inline void Execute()
	{
		if (FuncDelegate.IsBound())
		{
#if STATS
			TStatId StatId = TStatId();
			UObject* Object = FuncDelegate.GetUObject();
			if (Object)
			{
				StatId = Object->GetStatID();
			}
			FScopeCycleCounter Context(StatId);
#endif
			FuncDelegate.Execute();
		}
		else if (FuncDynDelegate.IsBound())
		{
			FuncDynDelegate.ProcessDelegate<UObject>(nullptr);
		}
		else if ( FuncCallback )
		{
			FuncCallback();
		}
	}

	inline bool IsBound() const
	{
		return ( FuncDelegate.IsBound() || FuncDynDelegate.IsBound() || FuncCallback );
	}

	inline bool IsBoundToObject(void const* Object) const
	{
		if (FuncDelegate.IsBound())
		{
			return FuncDelegate.IsBoundToObject(Object);
		}
		else if (FuncDynDelegate.IsBound())
		{
			return FuncDynDelegate.IsBoundToObject(Object);
		}

		return false;
	}

	inline void Unbind()
	{
		FuncDelegate.Unbind();
		FuncDynDelegate.Unbind();
		FuncCallback = nullptr;
	}

	/** Utility to output info about delegate as a string. */
	FString ToString() const;
};

enum class ETimerStatus : uint8
{
	Pending,
	Active,
	Paused,
	Executing
};

struct FTimerData
{
	/** If true, this timer will loop indefinitely.  Otherwise, it will be destroyed when it expires. */
	uint8 bLoop : 1;

	/** If true, this timer was created with a delegate to call (which means if the delegate becomes invalid, we should invalidate the timer too). */
	uint8 bRequiresDelegate : 1;

	/** Timer Status */
	ETimerStatus Status;
	
	/** Time between set and fire, or repeat frequency if looping. */
	float Rate;

	/** 
	 * Time (on the FTimerManager's clock) that this timer should expire and fire its delegate. 
	 * Note when a timer is paused, we re-base ExpireTime to be relative to 0 instead of the running clock, 
	 * meaning ExpireTime contains the remaining time until fire.
	 */
	double ExpireTime;

	/** Holds the delegate to call. */
	FTimerUnifiedDelegate TimerDelegate;

	FTimerHandle TimerHandle;

	/** The level collection that was active when this timer was created. Used to set the correct context before executing the timer's delegate. */
	ELevelCollectionType LevelCollection;

	FTimerData()
		: bLoop(false)
		, bRequiresDelegate(false)
		, Status(ETimerStatus::Active)
		, Rate(0)
		, ExpireTime(0)
		, LevelCollection(ELevelCollectionType::DynamicSourceLevels)
	{}

	/** Operator less, used to sort the heap based on time until execution. **/
	bool operator<(const FTimerData& Other) const
	{
		return ExpireTime < Other.ExpireTime;
	}

	void Clear()
	{
		TimerDelegate.Unbind();
		TimerHandle.Invalidate();
	}
};


/** 
 * Class to globally manage timers.
 */
class ENGINE_API FTimerManager : public FNoncopyable
{
public:

	void Tick(float DeltaTime);
	TStatId GetStatId() const;

	// ----------------------------------
	// Timer API

	FTimerManager();
	virtual ~FTimerManager();

	/**
	 * Called from crash handler to provide more debug information.
	 */
	virtual void OnCrash();

	/**
	 * Sets a timer to call the given native function at a set interval.  If a timer is already set
	 * for this delegate, it will update the current timer to the new parameters and reset its
	 * elapsed time to 0.
	 *
	 * @param InOutHandle			Handle to identify this timer. If it is invalid when passed in it will be made into a valid handle.
	 * @param InObj					Object to call the timer function on.
	 * @param InTimerMethod			Method to call when timer fires.
	 * @param InRate					The amount of time between set and firing.  If <= 0.f, clears existing timers.
	 * @param InbLoop				true to keep firing at Rate intervals, false to fire only once.
	 * @param InFirstDelay			The time for the first iteration of a looping timer. If < 0.f inRate will be used.
	 */
	template< class UserClass >
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay);
	}
	template< class UserClass >
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay);
	}

	/** Version that takes any generic delegate. */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDelegate), InRate, InbLoop, InFirstDelay);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDynamicDelegate const& InDynDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDynDelegate), InRate, InbLoop, InFirstDelay);
	}
	/*** Version that doesn't take a delegate */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(), InRate, InbLoop, InFirstDelay);
	}
	/** Version that takes a TFunction */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, TFunction<void(void)>&& Callback, float InRate, bool InbLoop, float InFirstDelay = -1.f )
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(MoveTemp(Callback)), InRate, InbLoop, InFirstDelay);
	}

	/**
	 * Sets a timer to call the given native function on the next tick.
	 *
	 * @param inObj					Object to call the timer function on.
	 * @param inTimerMethod			Method to call when timer fires.
	 */
	template< class UserClass >
	FORCEINLINE void SetTimerForNextTick(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
	}
	template< class UserClass >
	FORCEINLINE void SetTimerForNextTick(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
	}

	/** Version that takes any generic delegate. */
	FORCEINLINE void SetTimerForNextTick(FTimerDelegate const& InDelegate)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(InDelegate));
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	FORCEINLINE void SetTimerForNextTick(FTimerDynamicDelegate const& InDynDelegate)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(InDynDelegate));
	}
	/** Version that takes a TFunction */
	FORCEINLINE void SetTimerForNextTick(TFunction<void(void)>&& Callback)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(MoveTemp(Callback)));
	}

	/**
	 * DEPRECATED: Clears a previously set timer, identical to calling SetTimer() with a <= 0.f rate.
	 *
	 * @param InHandle The handle of the timer to clear.
	 */
	DEPRECATED(4.12, "This function is deprecated to ensure that timers that are no longer valid are not persisted. Please call this function with a non-const reference.")
	FORCEINLINE void ClearTimer(const FTimerHandle& InHandle)
	{
		InternalClearTimer(InHandle);
	}

	/**
	* Clears a previously set timer, identical to calling SetTimer() with a <= 0.f rate.
	* Invalidates the timer handle as it should no longer be used.
	*
	* @param InHandle The handle of the timer to clear.
	*/
	FORCEINLINE void ClearTimer(FTimerHandle& InHandle)
	{
		InternalClearTimer(InHandle);
		InHandle.Invalidate();
	}

	/** Clears all timers that are bound to functions on the given object. */
	FORCEINLINE void ClearAllTimersForObject(void const* Object)
	{
		if (Object)
		{
			InternalClearAllTimers( Object );
		}
	}

	/**
	 * Pauses a previously set timer.
	 *
	 * @param InHandle The handle of the timer to pause.
	 */
	FORCEINLINE void PauseTimer(FTimerHandle InHandle)
	{
		int32 TimerIdx;
		FTimerData const* TimerToPause = FindTimer(InHandle, &TimerIdx);
		InternalPauseTimer(TimerToPause, TimerIdx);
	}

	/**
	 * Unpauses a previously set timer
	 *
	 * @param InHandle The handle of the timer to unpause.
	 */
	FORCEINLINE void UnPauseTimer(FTimerHandle InHandle)
	{
		int32 TimerIdx = FindTimerInList(PausedTimerList, InHandle);
		InternalUnPauseTimer(TimerIdx);
	}

	/**
	 * Gets the current rate (time between activations) for the specified timer.
	 *
	 * @param InHandle The handle of the timer to return the rate of.
	 * @return The current rate or -1.f if timer does not exist.
	 */
	FORCEINLINE float GetTimerRate(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerRate(TimerData);
	}

	/**
	 * Returns true if the specified timer exists and is not paused
	 *
	 * @param InHandle The handle of the timer to check for being active.
	 * @return true if the timer exists and is active, false otherwise.
	 */
	FORCEINLINE bool IsTimerActive(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer( InHandle );
		return TimerData && TimerData->Status != ETimerStatus::Paused;
	}

	/**
	* Returns true if the specified timer exists and is paused
	*
	* @param InHandle The handle of the timer to check for being paused.
	* @return true if the timer exists and is paused, false otherwise.
	*/
	FORCEINLINE bool IsTimerPaused(FTimerHandle InHandle)
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return TimerData && TimerData->Status == ETimerStatus::Paused;
	}

	/**
	* Returns true if the specified timer exists and is pending
	*
	* @param InHandle The handle of the timer to check for being pending.
	* @return true if the timer exists and is pending, false otherwise.
	*/
	FORCEINLINE bool IsTimerPending(FTimerHandle InHandle)
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return TimerData && TimerData->Status == ETimerStatus::Pending;
	}

	/**
	* Returns true if the specified timer exists
	*
	* @param InHandle The handle of the timer to check for existence.
	* @return true if the timer exists, false otherwise.
	*/
	FORCEINLINE bool TimerExists(FTimerHandle InHandle)
	{
		return FindTimer(InHandle) != nullptr;
	}

	/**
	 * Gets the current elapsed time for the specified timer.
	 *
	 * @param InHandle The handle of the timer to check the elapsed time of.
	 * @return The current time elapsed or -1.f if the timer does not exist.
	 */
	FORCEINLINE float GetTimerElapsed(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerElapsed(TimerData);
	}

	/**
	 * Gets the time remaining before the specified timer is called
	 *
	 * @param InHandle The handle of the timer to check the remaining time of.
	 * @return	 The current time remaining, or -1.f if timer does not exist
	 */
	FORCEINLINE float GetTimerRemaining(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerRemaining(TimerData);
	}

	bool FORCEINLINE HasBeenTickedThisFrame() const
	{
		return (LastTickedFrame == GFrameCounter);
	}

	/**
	 * Finds a handle to a timer bound to a particular dynamic delegate.
	 * This function is intended to be used only by the K2 system.
	 *
	 * @param  InDynamicDelegate  The dynamic delegate to search for.
	 *
	 * @return A handle to the found timer - !IsValid() if no such timer was found.
	 */
	FTimerHandle K2_FindDynamicTimerHandle(FTimerDynamicDelegate InDynamicDelegate) const;

	/** Debug command to output info on all timers currently set to the log. */
	void ListTimers() const;

	/** Get the current last assigned handle */
	static void ValidateHandle(FTimerHandle& InOutHandle);

	/** Used by the UGameInstance constructor to set this manager's owning game instance. */
	void SetGameInstance(UGameInstance* InGameInstance) { OwningGameInstance = InGameInstance; }

private:
	void InternalSetTimer( FTimerHandle& InOutHandle, FTimerUnifiedDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay );
	void InternalSetTimer( FTimerData& NewTimerData, float InRate, bool InbLoop, float InFirstDelay );
	void InternalSetTimerForNextTick( FTimerUnifiedDelegate const& InDelegate );
	void InternalClearTimer( FTimerHandle const& InDelegate );
	void InternalClearTimer( int32 TimerIdx, ETimerStatus TimerStatus );
	void InternalClearAllTimers( void const* Object );

	/** Will find a timer in the active, paused, or pending list. */
	FTimerData const* FindTimer( FTimerHandle const& InHandle, int32* OutTimerIndex = nullptr ) const;

	void InternalPauseTimer( FTimerData const* TimerToPause, int32 TimerIdx );
	void InternalUnPauseTimer( int32 PausedTimerIdx );
	
	float InternalGetTimerRate( FTimerData const* const TimerData ) const;
	float InternalGetTimerElapsed( FTimerData const* const TimerData ) const;
	float InternalGetTimerRemaining( FTimerData const* const TimerData ) const;

	/** Will find the given timer in the given TArray and return its index. */ 
	int32 FindTimerInList( const TArray<FTimerData> &SearchArray, FTimerHandle const& InHandle ) const;

	/** Heap of actively running timers. */
	TArray<FTimerData> ActiveTimerHeap;
	/** Unordered list of paused timers. */
	TArray<FTimerData> PausedTimerList;
	/** List of timers added this frame, to be added after timer has been ticked */
	TArray<FTimerData> PendingTimerList;

	/** An internally consistent clock, independent of World.  Advances during ticking. */
	double InternalTime;

	/** Timer delegate currently being executed.  Used to handle "timer delegates that manipulate timers" cases. */
	FTimerData CurrentlyExecutingTimer;

	/** Set this to GFrameCounter when Timer is ticked. To figure out if Timer has been already ticked or not this frame. */
	uint64 LastTickedFrame;

	/** The last handle we assigned from this timer manager */
	static uint64 LastAssignedHandle;

	/** The game instance that created this timer manager. May be null if this timer manager wasn't created by a game instance. */
	UGameInstance* OwningGameInstance;
};

