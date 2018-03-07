// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/NoopCounter.h"
#include "Containers/ContainersFwd.h"
#include "PlatformProcess.h"

CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogLockFreeList, Log, All);

// what level of checking to perform...normally checkLockFreePointerList but could be ensure or check
#define checkLockFreePointerList checkSlow
//#define checkLockFreePointerList(x) ((x)||((*(char*)3) = 0)


#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

	CORE_API void DoTestCriticalStall();
	extern CORE_API int32 GTestCriticalStalls;

	FORCEINLINE void TestCriticalStall()
	{
		if (GTestCriticalStalls)
		{
			DoTestCriticalStall();
		}
	}
#else
	FORCEINLINE void TestCriticalStall()
	{
	}
#endif

CORE_API void LockFreeTagCounterHasOverflowed();
CORE_API void LockFreeLinksExhausted(uint32 TotalNum);
CORE_API void* LockFreeAllocLinks(SIZE_T AllocSize);
CORE_API void LockFreeFreeLinks(SIZE_T AllocSize, void* Ptr);

#define MAX_LOCK_FREE_LINKS_AS_BITS (26)
#define MAX_LOCK_FREE_LINKS (1 << 26)

template<int TPaddingForCacheContention>
struct FPaddingForCacheContention
{
	uint8 PadToAvoidContention[TPaddingForCacheContention];
};

template<>
struct FPaddingForCacheContention<0>
{
};

template<class T, unsigned int MaxTotalItems, unsigned int ItemsPerPage>
class TLockFreeAllocOnceIndexedAllocator
{
	enum
	{
		MaxBlocks = (MaxTotalItems + ItemsPerPage - 1) / ItemsPerPage
	};
public:

	TLockFreeAllocOnceIndexedAllocator()
	{
		NextIndex.Increment(); // skip the null ptr
		for (uint32 Index = 0; Index < MaxBlocks; Index++)
		{
			Pages[Index] = nullptr;
		}
	}

	FORCEINLINE uint32 Alloc(uint32 Count = 1)
	{
		uint32 FirstItem = NextIndex.Add(Count);
		if (FirstItem + Count > MaxTotalItems)
		{
			LockFreeLinksExhausted(MaxTotalItems);
		}
		for (uint32 CurrentItem = FirstItem; CurrentItem < FirstItem + Count; CurrentItem++)
		{
			new (GetRawItem(CurrentItem)) T();
		}
		return FirstItem;
	}
	FORCEINLINE T* GetItem(uint32 Index)
	{
		if (!Index)
		{
			return nullptr;
		}
		uint32 BlockIndex = Index / ItemsPerPage;
		uint32 SubIndex = Index % ItemsPerPage;
		checkLockFreePointerList(Index < (uint32)NextIndex.GetValue() && Index < MaxTotalItems && BlockIndex < MaxBlocks && Pages[BlockIndex]);
		return Pages[BlockIndex] + SubIndex;
	}

private:
	void* GetRawItem(uint32 Index)
	{
		uint32 BlockIndex = Index / ItemsPerPage;
		uint32 SubIndex = Index % ItemsPerPage;
		checkLockFreePointerList(Index && Index < (uint32)NextIndex.GetValue() && Index < MaxTotalItems && BlockIndex < MaxBlocks);
		if (!Pages[BlockIndex])
		{
			T* NewBlock = (T*)LockFreeAllocLinks(ItemsPerPage * sizeof(T));
			checkLockFreePointerList(IsAligned(NewBlock, alignof(T)));
			if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Pages[BlockIndex], NewBlock, nullptr) != nullptr)
			{
				// we lost discard block
				checkLockFreePointerList(Pages[BlockIndex] && Pages[BlockIndex] != NewBlock);
				LockFreeFreeLinks(ItemsPerPage * sizeof(T), NewBlock);
			}
			else
			{
				checkLockFreePointerList(Pages[BlockIndex]);
			}
		}
		return (void*)(Pages[BlockIndex] + SubIndex);
	}

	uint8 PadToAvoidContention0[PLATFORM_CACHE_LINE_SIZE];
	FThreadSafeCounter NextIndex;
	uint8 PadToAvoidContention1[PLATFORM_CACHE_LINE_SIZE];
	T* Pages[MaxBlocks];
	uint8 PadToAvoidContention2[PLATFORM_CACHE_LINE_SIZE];
};


#define MAX_TagBitsValue (uint64(1) << (64 - MAX_LOCK_FREE_LINKS_AS_BITS))
struct FIndexedLockFreeLink;


MS_ALIGN(8)
struct FIndexedPointer
{
	// no constructor, intentionally. We need to keep the ABA double counter in tact

	// This should only be used for FIndexedPointer's with no outstanding concurrency.
	// Not recycled links, for example.
	void Init()
	{
		static_assert(((MAX_LOCK_FREE_LINKS - 1) & MAX_LOCK_FREE_LINKS) == 0, "MAX_LOCK_FREE_LINKS must be a power of two");
		Ptrs = 0;
	}
	FORCEINLINE void SetAll(uint32 Ptr, uint64 CounterAndState)
	{
		checkLockFreePointerList(Ptr < MAX_LOCK_FREE_LINKS && CounterAndState < (uint64(1) << (64 - MAX_LOCK_FREE_LINKS_AS_BITS)));
		Ptrs = (uint64(Ptr) | (CounterAndState << MAX_LOCK_FREE_LINKS_AS_BITS));
	}

	FORCEINLINE uint32 GetPtr() const
	{
		return uint32(Ptrs & (MAX_LOCK_FREE_LINKS - 1));
	}

	FORCEINLINE void SetPtr(uint32 To)
	{
		SetAll(To, GetCounterAndState());
	}

	FORCEINLINE uint64 GetCounterAndState() const
	{
		return (Ptrs >> MAX_LOCK_FREE_LINKS_AS_BITS);
	}

	FORCEINLINE void SetCounterAndState(uint64 To)
	{
		SetAll(GetPtr(), To);
	}

	FORCEINLINE void AdvanceCounterAndState(const FIndexedPointer &From, uint64 TABAInc)
	{
		SetCounterAndState(From.GetCounterAndState() + TABAInc);
		if (UNLIKELY(GetCounterAndState() < From.GetCounterAndState()))
		{
			// this is not expected to be a problem and it is not expected to happen very often. When it does happen, we will sleep as an extra precaution.
			LockFreeTagCounterHasOverflowed();
		}
	}

	template<uint64 TABAInc>
	FORCEINLINE uint64 GetState() const
	{
		return GetCounterAndState() & (TABAInc - 1);
	}

	template<uint64 TABAInc>
	FORCEINLINE void SetState(uint64 Value)
	{
		checkLockFreePointerList(Value < TABAInc);
		SetCounterAndState((GetCounterAndState() & ~(TABAInc - 1)) | Value);
	}

	FORCEINLINE void AtomicRead(const FIndexedPointer& Other)
	{
		checkLockFreePointerList(IsAligned(&Ptrs, 8) && IsAligned(&Other.Ptrs, 8));
		Ptrs = uint64(FPlatformAtomics::AtomicRead64((volatile const int64*)&Other.Ptrs));
		TestCriticalStall();
	}

	FORCEINLINE bool InterlockedCompareExchange(const FIndexedPointer& Exchange, const FIndexedPointer& Comparand)
	{
		TestCriticalStall();
		return uint64(FPlatformAtomics::InterlockedCompareExchange((volatile int64*)&Ptrs, Exchange.Ptrs, Comparand.Ptrs)) == Comparand.Ptrs;
	}

	FORCEINLINE bool operator==(const FIndexedPointer& Other) const
	{
		return Ptrs == Other.Ptrs;
	}
	FORCEINLINE bool operator!=(const FIndexedPointer& Other) const
	{
		return Ptrs != Other.Ptrs;
	}

private:
	uint64 Ptrs;

} GCC_ALIGN(8);

struct FIndexedLockFreeLink
{
	FIndexedPointer DoubleNext;
	void *Payload;
	uint32 SingleNext;
};

// there is a version of this code that uses 128 bit atomics to avoid the indirection, that is why we have this policy class at all.
struct FLockFreeLinkPolicy
{
	enum
	{
		MAX_BITS_IN_TLinkPtr = MAX_LOCK_FREE_LINKS_AS_BITS
	};
	typedef FIndexedPointer TDoublePtr;
	typedef FIndexedLockFreeLink TLink;
	typedef uint32 TLinkPtr;
	typedef TLockFreeAllocOnceIndexedAllocator<FIndexedLockFreeLink, MAX_LOCK_FREE_LINKS, 16384> TAllocator;

	static FORCEINLINE FIndexedLockFreeLink* DerefLink(uint32 Ptr)
	{
		return LinkAllocator.GetItem(Ptr);
	}
	static FORCEINLINE FIndexedLockFreeLink* IndexToLink(uint32 Index)
	{
		return LinkAllocator.GetItem(Index);
	}
	static FORCEINLINE uint32 IndexToPtr(uint32 Index)
	{
		return Index;
	}

	CORE_API static uint32 AllocLockFreeLink();
	CORE_API static void FreeLockFreeLink(uint32 Item);
	CORE_API static TAllocator LinkAllocator;
};

template<int TPaddingForCacheContention, uint64 TABAInc = 1>
class FLockFreePointerListLIFORoot : public FNoncopyable
{
	typedef FLockFreeLinkPolicy::TDoublePtr TDoublePtr;
	typedef FLockFreeLinkPolicy::TLink TLink;
	typedef FLockFreeLinkPolicy::TLinkPtr TLinkPtr;

public:
	FORCEINLINE FLockFreePointerListLIFORoot()
	{
		// We want to make sure we have quite a lot of extra counter values to avoid the ABA problem. This could probably be relaxed, but eventually it will be dangerous. 
		// The question is "how many queue operations can a thread starve for".
		static_assert(MAX_TagBitsValue / TABAInc >= (1 << 23), "risk of ABA problem");
		static_assert((TABAInc & (TABAInc - 1)) == 0, "must be power of two");
		Reset();
	}

	void Reset()
	{
		Head.Init();
	}

	void Push(TLinkPtr Item)
	{
		while (true)
		{
			TDoublePtr LocalHead;
			LocalHead.AtomicRead(Head);
			TDoublePtr NewHead;
			NewHead.AdvanceCounterAndState(LocalHead, TABAInc);
			NewHead.SetPtr(Item);
			FLockFreeLinkPolicy::DerefLink(Item)->SingleNext = LocalHead.GetPtr();
			if (Head.InterlockedCompareExchange(NewHead, LocalHead))
			{
				break;
			}
		}
	}

	bool PushIf(TFunctionRef<TLinkPtr(uint64)> AllocateIfOkToPush)
	{
		static_assert(TABAInc > 1, "method should not be used for lists without state");
		while (true)
		{
			TDoublePtr LocalHead;
			LocalHead.AtomicRead(Head);
			uint64 LocalState = LocalHead.GetState<TABAInc>();
			TLinkPtr Item = AllocateIfOkToPush(LocalState);
			if (!Item)
			{
				return false;
			}

			TDoublePtr NewHead;
			NewHead.AdvanceCounterAndState(LocalHead, TABAInc);
			FLockFreeLinkPolicy::DerefLink(Item)->SingleNext = LocalHead.GetPtr();
			NewHead.SetPtr(Item);
			if (Head.InterlockedCompareExchange(NewHead, LocalHead))
			{
				break;
			}
		}
		return true;
	}


	TLinkPtr Pop()
	{
		TLinkPtr Item = 0;
		while (true)
		{
			TDoublePtr LocalHead;
			LocalHead.AtomicRead(Head);
			Item = LocalHead.GetPtr();
			if (!Item)
			{
				break;
			}
			TDoublePtr NewHead;
			NewHead.AdvanceCounterAndState(LocalHead, TABAInc);
			TLink* ItemP = FLockFreeLinkPolicy::DerefLink(Item);
			NewHead.SetPtr(ItemP->SingleNext);
			if (Head.InterlockedCompareExchange(NewHead, LocalHead))
			{
				ItemP->SingleNext = 0;
				break;
			}
		}
		return Item;
	}

	TLinkPtr PopAll()
	{
		TLinkPtr Item = 0;
		while (true)
		{
			TDoublePtr LocalHead;
			LocalHead.AtomicRead(Head);
			Item = LocalHead.GetPtr();
			if (!Item)
			{
				break;
			}
			TDoublePtr NewHead;
			NewHead.AdvanceCounterAndState(LocalHead, TABAInc);
			NewHead.SetPtr(0);
			if (Head.InterlockedCompareExchange(NewHead, LocalHead))
			{
				break;
			}
		}
		return Item;
	}

	TLinkPtr PopAllAndChangeState(TFunctionRef<uint64(uint64)> StateChange)
	{
		static_assert(TABAInc > 1, "method should not be used for lists without state");
		TLinkPtr Item = 0;
		while (true)
		{
			TDoublePtr LocalHead;
			LocalHead.AtomicRead(Head);
			Item = LocalHead.GetPtr();
			TDoublePtr NewHead;
			NewHead.AdvanceCounterAndState(LocalHead, TABAInc);
			NewHead.SetState<TABAInc>(StateChange(LocalHead.GetState<TABAInc>()));
			NewHead.SetPtr(0);
			if (Head.InterlockedCompareExchange(NewHead, LocalHead))
			{
				break;
			}
		}
		return Item;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return !Head.GetPtr();
	}

	FORCEINLINE uint64 GetState() const
	{
		TDoublePtr LocalHead;
		LocalHead.AtomicRead(Head);
		return LocalHead.GetState<TABAInc>();
	}

private:

	FPaddingForCacheContention<TPaddingForCacheContention> PadToAvoidContention1;
	TDoublePtr Head;
	FPaddingForCacheContention<TPaddingForCacheContention> PadToAvoidContention2;
};

template<class T, int TPaddingForCacheContention, uint64 TABAInc = 1>
class FLockFreePointerListLIFOBase : public FNoncopyable
{
	typedef FLockFreeLinkPolicy::TDoublePtr TDoublePtr;
	typedef FLockFreeLinkPolicy::TLink TLink;
	typedef FLockFreeLinkPolicy::TLinkPtr TLinkPtr;
public:
	void Reset()
	{
		RootList.Reset();
	}

	void Push(T* InPayload)
	{
		TLinkPtr Item = FLockFreeLinkPolicy::AllocLockFreeLink();
		FLockFreeLinkPolicy::DerefLink(Item)->Payload = InPayload;
		RootList.Push(Item);
	}

	bool PushIf(T* InPayload, TFunctionRef<bool(uint64)> OkToPush)
	{
		TLinkPtr Item = 0;

		auto AllocateIfOkToPush = [&OkToPush, InPayload, &Item](uint64 State)->TLinkPtr
		{
			if (OkToPush(State))
			{
				if (!Item)
				{
					Item = FLockFreeLinkPolicy::AllocLockFreeLink();
					FLockFreeLinkPolicy::DerefLink(Item)->Payload = InPayload;
				}
				return Item;
			}
			return 0;
		};
		if (!RootList.PushIf(AllocateIfOkToPush))
		{
			if (Item)
			{
				// we allocated the link, but it turned out that the list was closed
				FLockFreeLinkPolicy::FreeLockFreeLink(Item);
			}
			return false;
		}
		return true;
	}


	T* Pop()
	{
		TLinkPtr Item = RootList.Pop();
		T* Result = nullptr;
		if (Item)
		{
			Result = (T*)FLockFreeLinkPolicy::DerefLink(Item)->Payload;
			FLockFreeLinkPolicy::FreeLockFreeLink(Item);
		}
		return Result;
	}

	void PopAll(TArray<T*>& OutArray)
	{
		TLinkPtr Links = RootList.PopAll();
		while (Links)
		{
			TLink* LinksP = FLockFreeLinkPolicy::DerefLink(Links);
			OutArray.Add((T*)LinksP->Payload);
			TLinkPtr Del = Links;
			Links = LinksP->SingleNext;
			FLockFreeLinkPolicy::FreeLockFreeLink(Del);
		}
	}

	void PopAllAndChangeState(TArray<T*>& OutArray, TFunctionRef<uint64(uint64)> StateChange)
	{
		TLinkPtr Links = RootList.PopAllAndChangeState(StateChange);
		while (Links)
		{
			TLink* LinksP = FLockFreeLinkPolicy::DerefLink(Links);
			OutArray.Add((T*)LinksP->Payload);
			TLinkPtr Del = Links;
			Links = LinksP->SingleNext;
			FLockFreeLinkPolicy::FreeLockFreeLink(Del);
		}
	}

	FORCEINLINE bool IsEmpty() const
	{
		return RootList.IsEmpty();
	}

	FORCEINLINE uint64 GetState() const
	{
		return RootList.GetState();
	}

private:

	FLockFreePointerListLIFORoot<TPaddingForCacheContention, TABAInc> RootList;
};

template<class T, int TPaddingForCacheContention, uint64 TABAInc = 1>
class FLockFreePointerFIFOBase : public FNoncopyable
{
	typedef FLockFreeLinkPolicy::TDoublePtr TDoublePtr;
	typedef FLockFreeLinkPolicy::TLink TLink;
	typedef FLockFreeLinkPolicy::TLinkPtr TLinkPtr;
public:

	FORCEINLINE FLockFreePointerFIFOBase()
	{
		// We want to make sure we have quite a lot of extra counter values to avoid the ABA problem. This could probably be relaxed, but eventually it will be dangerous. 
		// The question is "how many queue operations can a thread starve for".
		static_assert(TABAInc <= 65536, "risk of ABA problem");
		static_assert((TABAInc & (TABAInc - 1)) == 0, "must be power of two");

		Head.Init();
		Tail.Init();
		TLinkPtr Stub = FLockFreeLinkPolicy::AllocLockFreeLink();
		Head.SetPtr(Stub);
		Tail.SetPtr(Stub);
	}

	void Push(T* InPayload)
	{
		TLinkPtr Item = FLockFreeLinkPolicy::AllocLockFreeLink();
		FLockFreeLinkPolicy::DerefLink(Item)->Payload = InPayload;
		TDoublePtr LocalTail;
		while (true)
		{
			LocalTail.AtomicRead(Tail);
			TLink* LoadTailP = FLockFreeLinkPolicy::DerefLink(LocalTail.GetPtr());
			TDoublePtr LocalNext;
			LocalNext.AtomicRead(LoadTailP->DoubleNext);
			TDoublePtr TestLocalTail;
			TestLocalTail.AtomicRead(Tail);
			if (TestLocalTail == LocalTail)
			{
				if (LocalNext.GetPtr())
				{
					TDoublePtr NewTail;
					NewTail.AdvanceCounterAndState(LocalTail, TABAInc);
					NewTail.SetPtr(LocalNext.GetPtr());
					Tail.InterlockedCompareExchange(NewTail, LocalTail);
				}
				else
				{
					TDoublePtr NewNext;
					NewNext.AdvanceCounterAndState(LocalNext, 1);
					NewNext.SetPtr(Item);
					if (LoadTailP->DoubleNext.InterlockedCompareExchange(NewNext, LocalNext))
					{
						break;
					}
				}
			}
		}
		{
			TestCriticalStall();
			TDoublePtr NewTail;
			NewTail.AdvanceCounterAndState(LocalTail, TABAInc);
			NewTail.SetPtr(Item);
			Tail.InterlockedCompareExchange(NewTail, LocalTail);
		}
	}

	T* Pop()
	{
		T* Result = nullptr;
		TDoublePtr LocalHead;
		while (true)
		{
			LocalHead.AtomicRead(Head);
			TDoublePtr LocalTail;
			LocalTail.AtomicRead(Tail);
			TDoublePtr LocalNext;
			LocalNext.AtomicRead(FLockFreeLinkPolicy::DerefLink(LocalHead.GetPtr())->DoubleNext);
			TDoublePtr LocalHeadTest;
			LocalHeadTest.AtomicRead(Head);
			if (LocalHead == LocalHeadTest)
			{
				if (LocalHead.GetPtr() == LocalTail.GetPtr())
				{
					if (!LocalNext.GetPtr())
					{
						return nullptr;
					}
					TestCriticalStall();
					TDoublePtr NewTail;
					NewTail.AdvanceCounterAndState(LocalTail, TABAInc);
					NewTail.SetPtr(LocalNext.GetPtr());
					Tail.InterlockedCompareExchange(NewTail, LocalTail);
				}
				else
				{
					Result = (T*)FLockFreeLinkPolicy::DerefLink(LocalNext.GetPtr())->Payload;
					TDoublePtr NewHead;
					NewHead.AdvanceCounterAndState(LocalHead, TABAInc);
					NewHead.SetPtr(LocalNext.GetPtr());
					if (Head.InterlockedCompareExchange(NewHead, LocalHead))
					{
						break;
					}
				}
			}
		}
		FLockFreeLinkPolicy::FreeLockFreeLink(LocalHead.GetPtr());
		return Result;
	}

	void PopAll(TArray<T*>& OutArray)
	{
		while (T* Item = Pop())
		{
			OutArray.Add(Item);
		}
	}


	FORCEINLINE bool IsEmpty() const
	{
		TDoublePtr LocalHead;
		LocalHead.AtomicRead(Head);
		TDoublePtr LocalNext;
		LocalNext.AtomicRead(FLockFreeLinkPolicy::DerefLink(LocalHead.GetPtr())->DoubleNext);
		return !LocalNext.GetPtr();
	}

private:

	FPaddingForCacheContention<TPaddingForCacheContention> PadToAvoidContention1;
	TDoublePtr Head;
	FPaddingForCacheContention<TPaddingForCacheContention> PadToAvoidContention2;
	TDoublePtr Tail;
	FPaddingForCacheContention<TPaddingForCacheContention> PadToAvoidContention3;
};


template<class T, int TPaddingForCacheContention, int NumPriorities>
class FStallingTaskQueue : public FNoncopyable
{
	typedef FLockFreeLinkPolicy::TDoublePtr TDoublePtr;
	typedef FLockFreeLinkPolicy::TLink TLink;
	typedef FLockFreeLinkPolicy::TLinkPtr TLinkPtr;
public:
	FStallingTaskQueue()
	{
		MasterState.Init();
	}
	int32 Push(T* InPayload, uint32 Priority)
	{
		checkLockFreePointerList(Priority < NumPriorities);
		TDoublePtr LocalMasterState;
		LocalMasterState.AtomicRead(MasterState);
		PriorityQueues[Priority].Push(InPayload);
		TDoublePtr NewMasterState;
		NewMasterState.AdvanceCounterAndState(LocalMasterState, 1);
		int32 ThreadToWake = FindThreadToWake(LocalMasterState.GetPtr());
		if (ThreadToWake >= 0)
		{
			NewMasterState.SetPtr(TurnOffBit(LocalMasterState.GetPtr(), ThreadToWake));
		}
		else
		{
			NewMasterState.SetPtr(LocalMasterState.GetPtr());
		}
		while (!MasterState.InterlockedCompareExchange(NewMasterState, LocalMasterState))
		{
			LocalMasterState.AtomicRead(MasterState);
			NewMasterState.AdvanceCounterAndState(LocalMasterState, 1);
			ThreadToWake = FindThreadToWake(LocalMasterState.GetPtr());
			if (ThreadToWake >= 0)
			{
				bool bAny = false;
				for (int32 Index = 0; !bAny && Index < NumPriorities; Index++)
				{
					bAny = PriorityQueues[Index].IsEmpty();
				}
				if (!bAny) // if there is nothing in the queues, then don't wake anyone
				{
					ThreadToWake = -1;
				}
			}
			if (ThreadToWake >= 0)
			{
				NewMasterState.SetPtr(TurnOffBit(LocalMasterState.GetPtr(), ThreadToWake));
			}
			else
			{
				NewMasterState.SetPtr(LocalMasterState.GetPtr());
			}
		}
		return ThreadToWake;
	}

	T* Pop(int32 MyThread, bool bAllowStall)
	{
		check(MyThread >= 0 && MyThread < FLockFreeLinkPolicy::MAX_BITS_IN_TLinkPtr);

		while (true)
		{
			TDoublePtr LocalMasterState;
			LocalMasterState.AtomicRead(MasterState);
			checkLockFreePointerList(!TestBit(LocalMasterState.GetPtr(), MyThread) || !FPlatformProcess::SupportsMultithreading()); // you should not be stalled if you are asking for a task
			for (int32 Index = 0; Index < NumPriorities; Index++)
			{
				T *Result = PriorityQueues[Index].Pop();
				if (Result)
				{
					return Result;
				}
			}
			if (!bAllowStall)
			{
				break; // if we aren't stalling, we are done, the queues are empty
			}
			TDoublePtr NewMasterState;
			NewMasterState.AdvanceCounterAndState(LocalMasterState, 1);
			NewMasterState.SetPtr(TurnOnBit(LocalMasterState.GetPtr(), MyThread));
			if (MasterState.InterlockedCompareExchange(NewMasterState, LocalMasterState))
			{
				break;
			}
		}
		return nullptr;
	}

private:

	static int32 FindThreadToWake(TLinkPtr Ptr)
	{
		int32 Result = -1;
		UPTRINT Test = UPTRINT(Ptr);
		if (Test)
		{
			Result = 0;
			while (!(Test & 1))
			{
				Test >>= 1;
				Result++;
			}
		}
		return Result;
	}

	static TLinkPtr TurnOffBit(TLinkPtr Ptr, int32 BitToTurnOff)
	{
		return (TLinkPtr)(UPTRINT(Ptr) & ~(UPTRINT(1) << BitToTurnOff));
	}

	static TLinkPtr TurnOnBit(TLinkPtr Ptr, int32 BitToTurnOn)
	{
		return (TLinkPtr)(UPTRINT(Ptr) | (UPTRINT(1) << BitToTurnOn));
	}

	static bool TestBit(TLinkPtr Ptr, int32 BitToTest)
	{
		return !!(UPTRINT(Ptr) & (UPTRINT(1) << BitToTest));
	}

	FLockFreePointerFIFOBase<T, TPaddingForCacheContention> PriorityQueues[NumPriorities];
	// not a pointer to anything, rather tracks the stall state of all threads servicing this queue.
	TDoublePtr MasterState;
	FPaddingForCacheContention<TPaddingForCacheContention> PadToAvoidContention1;
};




template<class T, int TPaddingForCacheContention>
class TLockFreePointerListLIFOPad : private FLockFreePointerListLIFOBase<T, TPaddingForCacheContention>
{
public:

	/**
	*	Push an item onto the head of the list.
	*
	*	@param NewItem, the new item to push on the list, cannot be NULL.
	*/
	void Push(T *NewItem)
	{
		FLockFreePointerListLIFOBase<T, TPaddingForCacheContention>::Push(NewItem);
	}

	/**
	*	Pop an item from the list or return NULL if the list is empty.
	*	@return The popped item, if any.
	*/
	T* Pop()
	{
		return FLockFreePointerListLIFOBase<T, TPaddingForCacheContention>::Pop();
	}

	/**
	*	Pop all items from the list.
	*
	*	@param Output The array to hold the returned items. Must be empty.
	*/
	void PopAll(TArray<T *>& Output)
	{
		FLockFreePointerListLIFOBase<T, TPaddingForCacheContention>::PopAll(Output);
	}

	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	FORCEINLINE bool IsEmpty() const
	{
		return FLockFreePointerListLIFOBase<T, TPaddingForCacheContention>::IsEmpty();
	}
};

template<class T>
class TLockFreePointerListLIFO : public TLockFreePointerListLIFOPad<T, 0>
{

};

template<class T, int TPaddingForCacheContention>
class TLockFreePointerListUnordered : public TLockFreePointerListLIFOPad<T, TPaddingForCacheContention>
{

};

template<class T, int TPaddingForCacheContention>
class TLockFreePointerListFIFO : private FLockFreePointerFIFOBase<T, TPaddingForCacheContention>
{
public:

	/**
	*	Push an item onto the head of the list.
	*
	*	@param NewItem, the new item to push on the list, cannot be NULL.
	*/
	void Push(T *NewItem)
	{
		FLockFreePointerFIFOBase<T, TPaddingForCacheContention>::Push(NewItem);
	}

	/**
	*	Pop an item from the list or return NULL if the list is empty.
	*	@return The popped item, if any.
	*/
	T* Pop()
	{
		return FLockFreePointerFIFOBase<T, TPaddingForCacheContention>::Pop();
	}

	/**
	*	Pop all items from the list.
	*
	*	@param Output The array to hold the returned items. Must be empty.
	*/
	void PopAll(TArray<T *>& Output)
	{
		FLockFreePointerFIFOBase<T, TPaddingForCacheContention>::PopAll(Output);
	}

	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	FORCEINLINE bool IsEmpty() const
	{
		return FLockFreePointerFIFOBase<T, TPaddingForCacheContention>::IsEmpty();
	}
};


template<class T, int TPaddingForCacheContention>
class TClosableLockFreePointerListUnorderedSingleConsumer : private FLockFreePointerListLIFOBase<T, TPaddingForCacheContention, 2>
{
public:

	/**
	*	Reset the list to the initial state. Not thread safe, but used for recycling when we know all users are gone.
	*/
	void Reset()
	{
		FLockFreePointerListLIFOBase<T, TPaddingForCacheContention, 2>::Reset();
	}

	/**
	*	Push an item onto the head of the list, unless the list is closed
	*
	*	@param NewItem, the new item to push on the list, cannot be NULL
	*	@return true if the item was pushed on the list, false if the list was closed.
	*/
	bool PushIfNotClosed(T *NewItem)
	{
		return FLockFreePointerListLIFOBase<T, TPaddingForCacheContention, 2>::PushIf(NewItem, [](uint64 State)->bool {return !(State & 1); });
	}

	/**
	*	Pop all items from the list and atomically close it.
	*
	*	@param Output The array to hold the returned items. Must be empty.
	*/
	void PopAllAndClose(TArray<T *>& Output)
	{
		auto CheckOpenAndClose = [](uint64 State) -> uint64
		{
			checkLockFreePointerList(!(State & 1));
			return State | 1;
		};
		FLockFreePointerListLIFOBase<T, TPaddingForCacheContention, 2>::PopAllAndChangeState(Output, CheckOpenAndClose);
	}

	/**
	*	Check if the list is closed
	*
	*	@return true if the list is closed.
	*/
	bool IsClosed() const
	{
		return !!(FLockFreePointerListLIFOBase<T, TPaddingForCacheContention, 2>::GetState() & 1);
	}

};


