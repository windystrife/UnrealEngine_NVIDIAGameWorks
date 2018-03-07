// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IPlatformFilePak.h"
#include "HAL/FileManager.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Async/AsyncWork.h"
#include "Serialization/MemoryReader.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/IPlatformFileModule.h"
#include "SignedArchiveReader.h"
#include "Misc/AES.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "Async/AsyncFileHandle.h"
#include "Templates/Greater.h"
#include "Serialization/ArchiveProxy.h"

DEFINE_LOG_CATEGORY(LogPakFile);

DEFINE_STAT(STAT_PakFile_Read);
DEFINE_STAT(STAT_PakFile_NumOpenHandles);

TPakChunkHash ComputePakChunkHash(const void* InData, int64 InDataSizeInBytes)
{
#if PAKHASH_USE_CRC
	return FCrc::MemCrc32(InData, InDataSizeInBytes);
#else
	FSHAHash Hash;
	FSHA1::HashBuffer(InData, InDataSizeInBytes, Hash);
	return Hash;
#endif
}

#ifndef EXCLUDE_NONPAK_UE_EXTENSIONS
#define EXCLUDE_NONPAK_UE_EXTENSIONS 1	// Use .Build.cs file to disable this if the game relies on accessing loose files
#endif

FFilenameSecurityDelegate& FPakPlatformFile::GetFilenameSecurityDelegate()
{
	static FFilenameSecurityDelegate Delegate;
	return Delegate;
}


#define USE_PAK_PRECACHE (!IS_PROGRAM && !WITH_EDITOR) // you can turn this off to use the async IO stuff without the precache

/**
* Precaching
*/

const ANSICHAR* FPakPlatformFile::GetPakEncryptionKey()
{
	FCoreDelegates::FPakEncryptionKeyDelegate& Delegate = FCoreDelegates::GetPakEncryptionKeyDelegate();
	if (Delegate.IsBound())
	{
		return Delegate.Execute();
	}
	else
	{
		return nullptr;
	}
}

void FPakPlatformFile::GetPakSigningKeys(FString& OutExponent, FString& OutModulus)
{
	FCoreDelegates::FPakSigningKeysDelegate& Delegate = FCoreDelegates::GetPakSigningKeysDelegate();
	if (Delegate.IsBound())
	{
		return Delegate.Execute(OutExponent, OutModulus);
	}
}

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("PakCache Sync Decrypts (Uncompressed Path)"), STAT_PakCache_SyncDecrypts, STATGROUP_PakFile);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("PakCache Decrypt Time"), STAT_PakCache_DecryptTime, STATGROUP_PakFile);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("PakCache Async Decrypts (Compressed Path)"), STAT_PakCache_CompressedDecrypts, STATGROUP_PakFile);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("PakCache Async Decrypts (Uncompressed Path)"), STAT_PakCache_UncompressedDecrypts, STATGROUP_PakFile);

inline void DecryptData(uint8* InData, uint32 InDataSize)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_PakCache_DecryptTime);
	const ANSICHAR* Key = FPakPlatformFile::GetPakEncryptionKey();
	checkf(Key, TEXT("AES decryption has been requested, but no valid encryption key was available"));
	FAES::DecryptData(InData, InDataSize, Key);
}

#if USE_PAK_PRECACHE
#include "TaskGraphInterfaces.h"
#define PAK_CACHE_GRANULARITY (64*1024)
static_assert((PAK_CACHE_GRANULARITY % FPakInfo::MaxChunkDataSize) == 0, "PAK_CACHE_GRANULARITY must be set to a multiple of FPakInfo::MaxChunkDataSize");
#define PAK_CACHE_MAX_REQUESTS (8)
#define PAK_CACHE_MAX_PRIORITY_DIFFERENCE_MERGE (AIOP_Normal - AIOP_Precache)
#define PAK_EXTRA_CHECKS DO_CHECK

DECLARE_MEMORY_STAT(TEXT("PakCache Current"), STAT_PakCacheMem, STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("PakCache High Water"), STAT_PakCacheHighWater, STATGROUP_Memory);

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("PakCache Signing Chunk Hash Time"), STAT_PakCache_SigningChunkHashTime, STATGROUP_PakFile);
DECLARE_MEMORY_STAT(TEXT("PakCache Signing Chunk Hash Size"), STAT_PakCache_SigningChunkHashSize, STATGROUP_PakFile);


static int32 GPakCache_Enable = 1;
static FAutoConsoleVariableRef CVar_Enable(
	TEXT("pakcache.Enable"),
	GPakCache_Enable,
	TEXT("If > 0, then enable the pak cache.")
	);

int32 GPakCache_MaxRequestsToLowerLevel = 2;
static FAutoConsoleVariableRef CVar_MaxRequestsToLowerLevel(
	TEXT("pakcache.MaxRequestsToLowerLevel"),
	GPakCache_MaxRequestsToLowerLevel,
	TEXT("Controls the maximum number of IO requests submitted to the OS filesystem at one time. Limited by PAK_CACHE_MAX_REQUESTS.")
	);

int32 GPakCache_MaxRequestSizeToLowerLevelKB = 1024;
static FAutoConsoleVariableRef CVar_MaxRequestSizeToLowerLevelKB(
	TEXT("pakcache.MaxRequestSizeToLowerLevellKB"),
	GPakCache_MaxRequestSizeToLowerLevelKB,
	TEXT("Controls the maximum size (in KB) of IO requests submitted to the OS filesystem.")
	);

int32 GPakCache_NumUnreferencedBlocksToCache = 10;
static FAutoConsoleVariableRef CVar_NumUnreferencedBlocksToCache(
	TEXT("pakcache.NumUnreferencedBlocksToCache"),
	GPakCache_NumUnreferencedBlocksToCache,
	TEXT("Controls the maximum number of unreferenced blocks to keep. This is a classic disk cache and the maxmimum wasted memory is pakcache.MaxRequestSizeToLowerLevellKB * pakcache.NumUnreferencedBlocksToCache.")
);

class FPakPrecacher;

typedef uint64 FJoinedOffsetAndPakIndex;
static FORCEINLINE uint16 GetRequestPakIndexLow(FJoinedOffsetAndPakIndex Joined)
{
	return uint16((Joined >> 48) & 0xffff);
}

static FORCEINLINE int64 GetRequestOffset(FJoinedOffsetAndPakIndex Joined)
{
	return int64(Joined & 0xffffffffffffll);
}

static FORCEINLINE FJoinedOffsetAndPakIndex MakeJoinedRequest(uint16 PakIndex, int64 Offset)
{
	check(Offset >= 0);
	return (FJoinedOffsetAndPakIndex(PakIndex) << 48) | Offset;
}

enum
{
	IntervalTreeInvalidIndex = 0
};


typedef uint32 TIntervalTreeIndex; // this is the arg type of TSparseArray::operator[]

static uint32 GNextSalt = 1;

// This is like TSparseArray, only a bit safer and I needed some restrictions on resizing.
template<class TItem>
class TIntervalTreeAllocator
{
	TArray<TItem> Items;
	TArray<int32> FreeItems; //@todo make this into a linked list through the existing items
	uint32 Salt;
	uint32 SaltMask;
public:
	TIntervalTreeAllocator()
	{
		check(GNextSalt < 4);
		Salt = (GNextSalt++) << 30;
		SaltMask = MAX_uint32 << 30;
		verify((Alloc() & ~SaltMask) == IntervalTreeInvalidIndex); // we want this to always have element zero so we can figure out an index from a pointer
	}
	inline TIntervalTreeIndex Alloc()
	{
		int32 Result;
		if (FreeItems.Num())
		{
			Result = FreeItems.Pop();
		}
		else
		{
			Result = Items.Num();
			Items.AddUninitialized();

		}
		new ((void*)&Items[Result]) TItem();
		return Result | Salt;;
	}
	void EnsureNoRealloc(int32 NeededNewNum)
	{
		if (FreeItems.Num() + Items.GetSlack() < NeededNewNum)
		{
			Items.Reserve(Items.Num() + NeededNewNum);
		}
	}
	FORCEINLINE TItem& Get(TIntervalTreeIndex InIndex)
	{
		TIntervalTreeIndex Index = InIndex & ~SaltMask;
		check((InIndex & SaltMask) == Salt && Index != IntervalTreeInvalidIndex && Index >= 0 && Index < (uint32)Items.Num()); //&& !FreeItems.Contains(Index));
		return Items[Index];
	}
	FORCEINLINE void Free(TIntervalTreeIndex InIndex)
	{
		TIntervalTreeIndex Index = InIndex & ~SaltMask;
		check((InIndex & SaltMask) == Salt && Index != IntervalTreeInvalidIndex && Index >= 0 && Index < (uint32)Items.Num()); //&& !FreeItems.Contains(Index));
		Items[Index].~TItem();
		FreeItems.Push(Index);
		if (FreeItems.Num() + 1 == Items.Num())
		{
			// get rid everything to restore memory coherence
			Items.Empty();
			FreeItems.Empty();
			verify((Alloc() & ~SaltMask) == IntervalTreeInvalidIndex); // we want this to always have element zero so we can figure out an index from a pointer
		}
	}
	FORCEINLINE void CheckIndex(TIntervalTreeIndex InIndex)
	{
		TIntervalTreeIndex Index = InIndex & ~SaltMask;
		check((InIndex & SaltMask) == Salt && Index != IntervalTreeInvalidIndex && Index >= 0 && Index < (uint32)Items.Num()); // && !FreeItems.Contains(Index));
	}
};

class FIntervalTreeNode
{
public:
	TIntervalTreeIndex LeftChildOrRootOfLeftList;
	TIntervalTreeIndex RootOfOnList;
	TIntervalTreeIndex RightChildOrRootOfRightList;

	FIntervalTreeNode()
		: LeftChildOrRootOfLeftList(IntervalTreeInvalidIndex)
		, RootOfOnList(IntervalTreeInvalidIndex)
		, RightChildOrRootOfRightList(IntervalTreeInvalidIndex)
	{
	}
	~FIntervalTreeNode()
	{
		check(LeftChildOrRootOfLeftList == IntervalTreeInvalidIndex && RootOfOnList == IntervalTreeInvalidIndex && RightChildOrRootOfRightList == IntervalTreeInvalidIndex); // this routine does not handle recursive destruction
	}
};

static TIntervalTreeAllocator<FIntervalTreeNode> GIntervalTreeNodeNodeAllocator;

static FORCEINLINE uint64 HighBit(uint64 x)
{
	return x & (1ull << 63);
}

static FORCEINLINE bool IntervalsIntersect(uint64 Min1, uint64 Max1, uint64 Min2, uint64 Max2)
{
	return !(Max2 < Min1 || Max1 < Min2);
}

template<typename TItem>
// this routine assume that the pointers remain valid even though we are reallocating
static void AddToIntervalTree_Dangerous(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint32 CurrentShift,
	uint32 MaxShift
	)
{
	while (true)
	{
		if (*RootNode == IntervalTreeInvalidIndex)
		{
			*RootNode = GIntervalTreeNodeNodeAllocator.Alloc();
		}

		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(*RootNode);

		if (MinShifted == MaxShifted && CurrentShift < MaxShift)
		{
			CurrentShift++;
			RootNode = (!MinShifted) ? &Root.LeftChildOrRootOfLeftList : &Root.RightChildOrRootOfRightList;
		}
		else
		{
			TItem& Item = Allocator.Get(Index);
			if (MinShifted != MaxShifted) // crosses middle
			{
				Item.Next = Root.RootOfOnList;
				Root.RootOfOnList = Index;
			}
			else // we are at the leaf
			{
				if (!MinShifted)
				{
					Item.Next = Root.LeftChildOrRootOfLeftList;
					Root.LeftChildOrRootOfLeftList = Index;
				}
				else
				{
					Item.Next = Root.RightChildOrRootOfRightList;
					Root.RightChildOrRootOfRightList = Index;
				}
			}
			return;
		}
	}
}

template<typename TItem>
static void AddToIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint32 StartShift,
	uint32 MaxShift
	)
{
	GIntervalTreeNodeNodeAllocator.EnsureNoRealloc(1 + MaxShift - StartShift);
	TItem& Item = Allocator.Get(Index);
	check(Item.Next == IntervalTreeInvalidIndex);
	uint64 MinInterval = GetRequestOffset(Item.OffsetAndPakIndex);
	uint64 MaxInterval = MinInterval + Item.Size - 1;
	AddToIntervalTree_Dangerous(RootNode, Allocator, Index, MinInterval, MaxInterval, StartShift, MaxShift);

}

template<typename TItem>
static FORCEINLINE bool ScanNodeListForRemoval(
	TIntervalTreeIndex* Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint64 MinInterval,
	uint64 MaxInterval
	)
{
	while (*Iter != IntervalTreeInvalidIndex)
	{

		TItem& Item = Allocator.Get(*Iter);
		if (*Iter == Index)
		{
			*Iter = Item.Next;
			Item.Next = IntervalTreeInvalidIndex;
			return true;
		}
		Iter = &Item.Next;
	}
	return false;
}

template<typename TItem>
static bool RemoveFromIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint32 CurrentShift,
	uint32 MaxShift
	)
{
	bool bResult = false;
	if (*RootNode != IntervalTreeInvalidIndex)
	{
		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(*RootNode);

		if (!MinShifted && !MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				bResult = ScanNodeListForRemoval(&Root.LeftChildOrRootOfLeftList, Allocator, Index, MinInterval, MaxInterval);
			}
			else
			{
				bResult = RemoveFromIntervalTree(&Root.LeftChildOrRootOfLeftList, Allocator, Index, MinInterval, MaxInterval, CurrentShift + 1, MaxShift);
			}
		}
		else if (!MinShifted && MaxShifted)
		{
			bResult = ScanNodeListForRemoval(&Root.RootOfOnList, Allocator, Index, MinInterval, MaxInterval);
		}
		else
		{
			if (CurrentShift == MaxShift)
			{
				bResult = ScanNodeListForRemoval(&Root.RightChildOrRootOfRightList, Allocator, Index, MinInterval, MaxInterval);
			}
			else
			{
				bResult = RemoveFromIntervalTree(&Root.RightChildOrRootOfRightList, Allocator, Index, MinInterval, MaxInterval, CurrentShift + 1, MaxShift);
			}
		}
		if (bResult)
		{
			if (Root.LeftChildOrRootOfLeftList == IntervalTreeInvalidIndex && Root.RootOfOnList == IntervalTreeInvalidIndex && Root.RightChildOrRootOfRightList == IntervalTreeInvalidIndex)
			{
				check(&Root == &GIntervalTreeNodeNodeAllocator.Get(*RootNode));
				GIntervalTreeNodeNodeAllocator.Free(*RootNode);
				*RootNode = IntervalTreeInvalidIndex;
			}
		}
	}
	return bResult;
}

template<typename TItem>
static bool RemoveFromIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint32 StartShift,
	uint32 MaxShift
	)
{
	TItem& Item = Allocator.Get(Index);
	uint64 MinInterval = GetRequestOffset(Item.OffsetAndPakIndex);
	uint64 MaxInterval = MinInterval + Item.Size - 1;
	return RemoveFromIntervalTree(RootNode, Allocator, Index, MinInterval, MaxInterval, StartShift, MaxShift);
}

template<typename TItem>
static FORCEINLINE void ScanNodeListForRemovalFunc(
	TIntervalTreeIndex* Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
	)
{
	while (*Iter != IntervalTreeInvalidIndex)
	{
		TItem& Item = Allocator.Get(*Iter);
		uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
		uint64 LastByte = Offset + uint64(Item.Size) - 1;

		// save the value and then clear it.
		TIntervalTreeIndex NextIndex = Item.Next;
		if (IntervalsIntersect(MinInterval, MaxInterval, Offset, LastByte) && Func(*Iter))
		{
			*Iter = NextIndex; // this may have already be deleted, so cannot rely on the memory block
		}
		else
		{
			Iter = &Item.Next;
		}
	}
}

template<typename TItem>
static void MaybeRemoveOverlappingNodesInIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
	)
{
	if (*RootNode != IntervalTreeInvalidIndex)
	{
		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(*RootNode);
		uint64 Center = (MinNode + MaxNode + 1) >> 1;

		//UE_LOG(LogTemp, Warning, TEXT("Exploring Node %X [%d, %d] %d%d     interval %llX %llX    node interval %llX %llX   center %llX  "), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted, MinInterval, MaxInterval, MinNode, MaxNode, Center);


		if (!MinShifted)
		{
			if (CurrentShift == MaxShift)
			{
				//UE_LOG(LogTemp, Warning, TEXT("LeftBottom %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				ScanNodeListForRemovalFunc(&Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, Func);
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("LeftRecur %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				MaybeRemoveOverlappingNodesInIntervalTree(&Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, FMath::Min(MaxInterval, Center - 1), MinNode, Center - 1, CurrentShift + 1, MaxShift, Func);
			}
		}

		//UE_LOG(LogTemp, Warning, TEXT("Center %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
		ScanNodeListForRemovalFunc(&Root.RootOfOnList, Allocator, MinInterval, MaxInterval, Func);

		if (MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				//UE_LOG(LogTemp, Warning, TEXT("RightBottom %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				ScanNodeListForRemovalFunc(&Root.RightChildOrRootOfRightList, Allocator, MinInterval, MaxInterval, Func);
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("RightRecur %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				MaybeRemoveOverlappingNodesInIntervalTree(&Root.RightChildOrRootOfRightList, Allocator, FMath::Max(MinInterval, Center), MaxInterval, Center, MaxNode, CurrentShift + 1, MaxShift, Func);
			}
		}

		//UE_LOG(LogTemp, Warning, TEXT("Done Exploring Node %X [%d, %d] %d%d     interval %llX %llX    node interval %llX %llX   center %llX  "), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted, MinInterval, MaxInterval, MinNode, MaxNode, Center);

		if (Root.LeftChildOrRootOfLeftList == IntervalTreeInvalidIndex && Root.RootOfOnList == IntervalTreeInvalidIndex && Root.RightChildOrRootOfRightList == IntervalTreeInvalidIndex)
		{
			check(&Root == &GIntervalTreeNodeNodeAllocator.Get(*RootNode));
			GIntervalTreeNodeNodeAllocator.Free(*RootNode);
			*RootNode = IntervalTreeInvalidIndex;
		}
	}
}


template<typename TItem>
static FORCEINLINE bool ScanNodeList(
	TIntervalTreeIndex Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
	)
{
	while (Iter != IntervalTreeInvalidIndex)
	{
		TItem& Item = Allocator.Get(Iter);
		uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
		uint64 LastByte = Offset + uint64(Item.Size) - 1;
		if (IntervalsIntersect(MinInterval, MaxInterval, Offset, LastByte))
		{
			if (!Func(Iter))
			{
				return false;
			}
		}
		Iter = Item.Next;
	}
	return true;
}

template<typename TItem>
static bool OverlappingNodesInIntervalTree(
	TIntervalTreeIndex RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
	)
{
	if (RootNode != IntervalTreeInvalidIndex)
	{
		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(RootNode);
		uint64 Center = (MinNode + MaxNode + 1) >> 1;

		if (!MinShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeList(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTree(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, FMath::Min(MaxInterval, Center - 1), MinNode, Center - 1, CurrentShift + 1, MaxShift, Func))
				{
					return false;
				}
			}
		}
		if (!ScanNodeList(Root.RootOfOnList, Allocator, MinInterval, MaxInterval, Func))
		{
			return false;
		}
		if (MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeList(Root.RightChildOrRootOfRightList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTree(Root.RightChildOrRootOfRightList, Allocator, FMath::Max(MinInterval, Center), MaxInterval, Center, MaxNode, CurrentShift + 1, MaxShift, Func))
				{
					return false;
				}
			}
		}
	}
	return true;
}

template<typename TItem>
static bool ScanNodeListWithShrinkingInterval(
	TIntervalTreeIndex Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64& MaxInterval,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
	)
{
	while (Iter != IntervalTreeInvalidIndex)
	{
		TItem& Item = Allocator.Get(Iter);
		uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
		uint64 LastByte = Offset + uint64(Item.Size) - 1;
		//UE_LOG(LogTemp, Warning, TEXT("Test Overlap %llu %llu %llu %llu"), MinInterval, MaxInterval, Offset, LastByte);
		if (IntervalsIntersect(MinInterval, MaxInterval, Offset, LastByte))
		{
			//UE_LOG(LogTemp, Warning, TEXT("Overlap %llu %llu %llu %llu"), MinInterval, MaxInterval, Offset, LastByte);
			if (!Func(Iter))
			{
				return false;
			}
		}
		Iter = Item.Next;
	}
	return true;
}

template<typename TItem>
static bool OverlappingNodesInIntervalTreeWithShrinkingInterval(
	TIntervalTreeIndex RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64& MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
	)
{
	if (RootNode != IntervalTreeInvalidIndex)
	{

		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(FMath::Min(MaxInterval, MaxNode) << CurrentShift); // since MaxInterval is changing, we cannot clamp it during recursion.
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(RootNode);
		uint64 Center = (MinNode + MaxNode + 1) >> 1;

		if (!MinShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeListWithShrinkingInterval(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTreeWithShrinkingInterval(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, MinNode, Center - 1, CurrentShift + 1, MaxShift, Func)) // since MaxInterval is changing, we cannot clamp it during recursion.
				{
					return false;
				}
			}
		}
		if (!ScanNodeListWithShrinkingInterval(Root.RootOfOnList, Allocator, MinInterval, MaxInterval, Func))
		{
			return false;
		}
		MaxShifted = HighBit(FMath::Min(MaxInterval, MaxNode) << CurrentShift); // since MaxInterval is changing, we cannot clamp it during recursion.
		if (MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeListWithShrinkingInterval(Root.RightChildOrRootOfRightList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTreeWithShrinkingInterval(Root.RightChildOrRootOfRightList, Allocator, FMath::Max(MinInterval, Center), MaxInterval, Center, MaxNode, CurrentShift + 1, MaxShift, Func))
				{
					return false;
				}
			}
		}
	}
	return true;
}


template<typename TItem>
static void MaskInterval(
	TIntervalTreeIndex Index,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint32 BytesToBitsShift,
	uint64* Bits
	)
{
	TItem& Item = Allocator.Get(Index);
	uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
	uint64 LastByte = Offset + uint64(Item.Size) - 1;
	uint64 InterMinInterval = FMath::Max(MinInterval, Offset);
	uint64 InterMaxInterval = FMath::Min(MaxInterval, LastByte);
	if (InterMinInterval <= InterMaxInterval)
	{
		uint32 FirstBit = uint32((InterMinInterval - MinInterval) >> BytesToBitsShift);
		uint32 LastBit = uint32((InterMaxInterval - MinInterval) >> BytesToBitsShift);
		uint32 FirstQWord = FirstBit >> 6;
		uint32 LastQWord = LastBit >> 6;
		uint32 FirstBitQWord = FirstBit & 63;
		uint32 LastBitQWord = LastBit & 63;
		if (FirstQWord == LastQWord)
		{
			Bits[FirstQWord] |= ((MAX_uint64 << FirstBitQWord) & (MAX_uint64 >> (63 - LastBitQWord)));
		}
		else
		{
			Bits[FirstQWord] |= (MAX_uint64 << FirstBitQWord);
			for (uint32 QWordIndex = FirstQWord + 1; QWordIndex < LastQWord; QWordIndex++)
			{
				Bits[QWordIndex] = MAX_uint64;
			}
			Bits[LastQWord] |= (MAX_uint64 >> (63 - LastBitQWord));
		}
	}
}



template<typename TItem>
static void OverlappingNodesInIntervalTreeMask(
	TIntervalTreeIndex RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	uint32 BytesToBitsShift,
	uint64* Bits
	)
{
	OverlappingNodesInIntervalTree(
		RootNode,
		Allocator,
		MinInterval,
		MaxInterval,
		MinNode,
		MaxNode,
		CurrentShift,
		MaxShift,
		[&Allocator, MinInterval, MaxInterval, BytesToBitsShift, Bits](TIntervalTreeIndex Index) -> bool
		{
			MaskInterval(Index, Allocator, MinInterval, MaxInterval, BytesToBitsShift, Bits);
			return true;
		}
	);
}



class IPakRequestor
{
	friend class FPakPrecacher;
	FJoinedOffsetAndPakIndex OffsetAndPakIndex; // this is used for searching and filled in when you make the request
	uint64 UniqueID;
	TIntervalTreeIndex InRequestIndex;
public:
	IPakRequestor()
		: OffsetAndPakIndex(MAX_uint64) // invalid value
		, UniqueID(0)
		, InRequestIndex(IntervalTreeInvalidIndex)
	{
	}
	virtual ~IPakRequestor()
	{
	}
	virtual void RequestIsComplete()
	{
	}
};

static FPakPrecacher* PakPrecacherSingleton = nullptr;

class FPakPrecacher
{
	enum class EInRequestStatus
	{
		Complete,
		Waiting,
		InFlight,
		Num
	};

	enum class EBlockStatus
	{
		InFlight,
		Complete,
		Num
	};

	IPlatformFile* LowerLevel;
	FCriticalSection CachedFilesScopeLock;
	FJoinedOffsetAndPakIndex LastReadRequest;
	uint64 NextUniqueID;
	int64 BlockMemory;
	int64 BlockMemoryHighWater;

	struct FCacheBlock
	{
		FJoinedOffsetAndPakIndex OffsetAndPakIndex;
		int64 Size;
		uint8 *Memory;
		uint32 InRequestRefCount;
		TIntervalTreeIndex Index;
		TIntervalTreeIndex Next;
		EBlockStatus Status;

		FCacheBlock()
			: OffsetAndPakIndex(0)
			, Size(0)
			, Memory(nullptr)
			, InRequestRefCount(0)
			, Index(IntervalTreeInvalidIndex)
			, Next(IntervalTreeInvalidIndex)
			, Status(EBlockStatus::InFlight)
		{
		}
	};

	struct FPakInRequest
	{
		FJoinedOffsetAndPakIndex OffsetAndPakIndex;
		int64 Size;
		IPakRequestor* Owner;
		uint64 UniqueID;
		TIntervalTreeIndex Index;
		TIntervalTreeIndex Next;
		EAsyncIOPriority Priority;
		EInRequestStatus Status;

		FPakInRequest()
			: OffsetAndPakIndex(0)
			, Size(0)
			, Owner(nullptr)
			, UniqueID(0)
			, Index(IntervalTreeInvalidIndex)
			, Next(IntervalTreeInvalidIndex)
			, Priority(AIOP_MIN)
			, Status(EInRequestStatus::Waiting)
		{
		}
	};

	struct FPakData
	{
		IAsyncReadFileHandle* Handle;
		int64 TotalSize;
		uint64 MaxNode;
		uint32 StartShift;
		uint32 MaxShift;
		uint32 BytesToBitsShift;
		FName Name;

		TIntervalTreeIndex InRequests[AIOP_NUM][(int32)EInRequestStatus::Num];
		TIntervalTreeIndex CacheBlocks[(int32)EBlockStatus::Num];

		TArray<TPakChunkHash> ChunkHashes;
		TPakChunkHash OriginalSignatureFileHash;

		FPakData(IAsyncReadFileHandle* InHandle, FName InName, int64 InTotalSize)
			: Handle(InHandle)
			, TotalSize(InTotalSize)
			, StartShift(0)
			, MaxShift(0)
			, BytesToBitsShift(0)
			, Name(InName)
		{
			check(Handle && TotalSize > 0 && Name != NAME_None);
			for (int32 Index = 0; Index < AIOP_NUM; Index++)
			{
				for (int32 IndexInner = 0; IndexInner < (int32)EInRequestStatus::Num; IndexInner++)
				{
					InRequests[Index][IndexInner] = IntervalTreeInvalidIndex;
				}
			}
			for (int32 IndexInner = 0; IndexInner < (int32)EBlockStatus::Num; IndexInner++)
			{
				CacheBlocks[IndexInner] = IntervalTreeInvalidIndex;
			}
			uint64 StartingLastByte = FMath::Max((uint64)TotalSize, uint64(PAK_CACHE_GRANULARITY + 1));
			StartingLastByte--;

			{
				uint64 LastByte = StartingLastByte;
				while (!HighBit(LastByte))
				{
					LastByte <<= 1;
					StartShift++;
				}
			}
			{
				uint64 LastByte = StartingLastByte;
				uint64 Block = (uint64)PAK_CACHE_GRANULARITY;

				while (Block)
				{
					Block >>= 1;
					LastByte >>= 1;
					BytesToBitsShift++;
				}
				BytesToBitsShift--;
				check(1 << BytesToBitsShift == PAK_CACHE_GRANULARITY);
				MaxShift = StartShift;
				while (LastByte)
				{
					LastByte >>= 1;
					MaxShift++;
				}
				MaxNode = MAX_uint64 >> StartShift;
				check(MaxNode >= StartingLastByte && (MaxNode >> 1) < StartingLastByte);
//				UE_LOG(LogTemp, Warning, TEXT("Test %d %llX %llX "), MaxShift, (uint64(PAK_CACHE_GRANULARITY) << (MaxShift + 1)), (uint64(PAK_CACHE_GRANULARITY) << MaxShift));
				check(MaxShift && (uint64(PAK_CACHE_GRANULARITY) << (MaxShift + 1)) == 0 && (uint64(PAK_CACHE_GRANULARITY) << MaxShift) != 0);
			}
		}
	};
	TMap<FName, uint16> CachedPaks;
	TArray<FPakData> CachedPakData;

	TIntervalTreeAllocator<FPakInRequest> InRequestAllocator;
	TIntervalTreeAllocator<FCacheBlock> CacheBlockAllocator;
	TMap<uint64, TIntervalTreeIndex> OutstandingRequests;

	TArray<FJoinedOffsetAndPakIndex> OffsetAndPakIndexOfSavedBlocked;

	struct FRequestToLower
	{
		IAsyncReadRequest* RequestHandle;
		TIntervalTreeIndex BlockIndex;
		int64 RequestSize;
		uint8* Memory;
		FRequestToLower()
			: RequestHandle(nullptr)
			, BlockIndex(IntervalTreeInvalidIndex)
			, RequestSize(0)
			, Memory(nullptr)
		{
		}
	};

	FRequestToLower RequestsToLower[PAK_CACHE_MAX_REQUESTS];
	TArray<IAsyncReadRequest*> RequestsToDelete;
	int32 NotifyRecursion;

	uint32 Loads;
	uint32 Frees;
	uint64 LoadSize;
	FEncryptionKey EncryptionKey;
	bool bSigned;

public:

	static void Init(IPlatformFile* InLowerLevel, const FEncryptionKey& InEncryptionKey)
	{
		if (!PakPrecacherSingleton)
		{
			verify(!FPlatformAtomics::InterlockedCompareExchangePointer((void**)&PakPrecacherSingleton, new FPakPrecacher(InLowerLevel, InEncryptionKey), nullptr));
		}
		check(PakPrecacherSingleton);
	}

	static void Shutdown()
	{
		if (PakPrecacherSingleton)
		{
			FPakPrecacher* LocalPakPrecacherSingleton = PakPrecacherSingleton;
			if (LocalPakPrecacherSingleton && LocalPakPrecacherSingleton == FPlatformAtomics::InterlockedCompareExchangePointer((void**)&PakPrecacherSingleton, nullptr, LocalPakPrecacherSingleton))
			{
				LocalPakPrecacherSingleton->TrimCache(true);
				double StartTime = FPlatformTime::Seconds();
				while (!LocalPakPrecacherSingleton->IsProbablyIdle())
				{
					FPlatformProcess::SleepNoStats(0.001f);
					if (FPlatformTime::Seconds() - StartTime > 10.0)
					{
						UE_LOG(LogPakFile, Error, TEXT("FPakPrecacher was not idle after 10s, exiting anyway and leaking."));
						return;
					}
				}
				delete PakPrecacherSingleton;
			}
		}
		check(!PakPrecacherSingleton);
	}

	static FPakPrecacher& Get()
	{
		check(PakPrecacherSingleton);
		return *PakPrecacherSingleton;
	}

	FPakPrecacher(IPlatformFile* InLowerLevel, const FEncryptionKey& InEncryptionKey)
		: LowerLevel(InLowerLevel)
		, LastReadRequest(0)
		, NextUniqueID(1)
		, BlockMemory(0)
		, BlockMemoryHighWater(0)
		, NotifyRecursion(0)
		, Loads(0)
		, Frees(0)
		, LoadSize(0)
		, EncryptionKey(InEncryptionKey)
		, bSigned(!InEncryptionKey.Exponent.IsZero() && !InEncryptionKey.Modulus.IsZero())
	{
		check(LowerLevel && FPlatformProcess::SupportsMultithreading());
		GPakCache_MaxRequestsToLowerLevel = FMath::Max(FMath::Min(FPlatformMisc::NumberOfIOWorkerThreadsToSpawn(), GPakCache_MaxRequestsToLowerLevel), 1);
		check(GPakCache_MaxRequestsToLowerLevel <= PAK_CACHE_MAX_REQUESTS);
	}

	void StartSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 IndexToFill);
	void DoSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 IndexToFill);

	IPlatformFile* GetLowerLevelHandle()
	{
		check(LowerLevel);
		return LowerLevel;
	}

	bool HasEnoughRoomForPrecache()
	{
		return GPakCache_AcceptPrecacheRequests;
	}

	uint16* RegisterPakFile(FName File, int64 PakFileSize)
	{
		uint16* PakIndexPtr = CachedPaks.Find(File);
		if (!PakIndexPtr)
		{
			check(CachedPakData.Num() < MAX_uint16);
			IAsyncReadFileHandle* Handle = LowerLevel->OpenAsyncRead(*File.ToString());
			if (!Handle)
			{
				return nullptr;
			}
			CachedPakData.Add(FPakData(Handle, File, PakFileSize));
			PakIndexPtr = &CachedPaks.Add(File, CachedPakData.Num() - 1);
			UE_LOG(LogPakFile, Log, TEXT("New pak file %s added to pak precacher."), *File.ToString());

			FPakData& Pak = CachedPakData[*PakIndexPtr];

			if (bSigned)
			{
				// Load signature data
				FString SignaturesFilename = FPaths::ChangeExtension(File.ToString(), TEXT("sig"));
				IFileHandle* SignaturesFile = LowerLevel->OpenRead(*SignaturesFilename);
				ensure(SignaturesFile);

				FArchiveFileReaderGeneric* Reader = new FArchiveFileReaderGeneric(SignaturesFile, *SignaturesFilename, SignaturesFile->Size());
				FEncryptedSignature MasterSignature;
				*Reader << MasterSignature;
				*Reader << Pak.ChunkHashes;
				delete Reader;

				// Check that we have the correct match between signature and pre-cache granularity
				int64 NumPakChunks = Align(PakFileSize, FPakInfo::MaxChunkDataSize) / FPakInfo::MaxChunkDataSize;
				ensure(NumPakChunks == Pak.ChunkHashes.Num());

				// Decrypt signature hash
				FDecryptedSignature DecryptedSignature;
				FEncryption::DecryptSignature(MasterSignature, DecryptedSignature, EncryptionKey);

				// Check the signatures are still as we expected them
				Pak.OriginalSignatureFileHash = ComputePakChunkHash(&Pak.ChunkHashes[0], Pak.ChunkHashes.Num() * sizeof(TPakChunkHash));
				ensure(Pak.OriginalSignatureFileHash == DecryptedSignature.Data);
			}
		}
		return PakIndexPtr;
	}

#if !UE_BUILD_SHIPPING
	void SimulatePakFileCorruption()
	{
		FScopeLock Lock(&CachedFilesScopeLock);
		
		for (FPakData& PakData : CachedPakData)
		{
			for (TPakChunkHash& Hash : PakData.ChunkHashes)
			{
				Hash |= (uint32)FMath::Rand();
				Hash &= (uint32)FMath::Rand();
			}
		}
	}
#endif

private: // below here we assume CachedFilesScopeLock until we get to the next section

	uint16 GetRequestPakIndex(FJoinedOffsetAndPakIndex OffsetAndPakIndex)
	{
		uint16 Result = GetRequestPakIndexLow(OffsetAndPakIndex);
		check(Result < CachedPakData.Num());
		return Result;
	}

	FJoinedOffsetAndPakIndex FirstUnfilledBlockForRequest(TIntervalTreeIndex NewIndex, FJoinedOffsetAndPakIndex ReadHead = 0)
	{
		// CachedFilesScopeLock is locked
		FPakInRequest& Request = InRequestAllocator.Get(NewIndex);
		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		int64 Size = Request.Size;
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Size > 0 && Request.Priority >= AIOP_MIN && Request.Priority <= AIOP_MAX && Request.Status != EInRequestStatus::Complete && Request.Owner);
		if (PakIndex != GetRequestPakIndex(ReadHead))
		{
			// this is in a different pak, so we ignore the read head position
			ReadHead = 0;
		}
		if (ReadHead)
		{
			// trim to the right of the read head
			int64 Trim = FMath::Max(Offset, GetRequestOffset(ReadHead)) - Offset;
			Offset += Trim;
			Size -= Trim;
		}

		static TArray<uint64> InFlightOrDone;

		int64 FirstByte = AlignDown(Offset, PAK_CACHE_GRANULARITY);
		int64 LastByte = Align(Offset + Size, PAK_CACHE_GRANULARITY) - 1;
		uint32 NumBits = (PAK_CACHE_GRANULARITY + LastByte - FirstByte) / PAK_CACHE_GRANULARITY;
		uint32 NumQWords = (NumBits + 63) >> 6;
		InFlightOrDone.Reset();
		InFlightOrDone.AddZeroed(NumQWords);
		if (NumBits != NumQWords * 64)
		{
			uint32 Extras = NumQWords * 64 - NumBits;
			InFlightOrDone[NumQWords - 1] = (MAX_uint64 << (64 - Extras));
		}

		if (Pak.CacheBlocks[(int32)EBlockStatus::Complete] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}
		if (Request.Status == EInRequestStatus::Waiting && Pak.CacheBlocks[(int32)EBlockStatus::InFlight] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}
		for (uint32 Index = 0; Index < NumQWords; Index++)
		{
			if (InFlightOrDone[Index] != MAX_uint64)
			{
				uint64 Mask = InFlightOrDone[Index];
				int64 FinalOffset = FirstByte + PAK_CACHE_GRANULARITY * 64 * Index;
				while (Mask & 1)
				{
					FinalOffset += PAK_CACHE_GRANULARITY;
					Mask >>= 1;
				}
				return MakeJoinedRequest(PakIndex, FinalOffset);
			}
		}
		return MAX_uint64;
	}

	bool AddRequest(TIntervalTreeIndex NewIndex)
	{
		// CachedFilesScopeLock is locked
		FPakInRequest& Request = InRequestAllocator.Get(NewIndex);
		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Request.Size > 0 && Request.Priority >= AIOP_MIN && Request.Priority <= AIOP_MAX && Request.Status == EInRequestStatus::Waiting && Request.Owner);

		static TArray<uint64> InFlightOrDone;

		int64 FirstByte = AlignDown(Offset, PAK_CACHE_GRANULARITY);
		int64 LastByte = Align(Offset + Request.Size, PAK_CACHE_GRANULARITY) - 1;
		uint32 NumBits = (PAK_CACHE_GRANULARITY + LastByte - FirstByte) / PAK_CACHE_GRANULARITY;
		uint32 NumQWords = (NumBits + 63) >> 6;
		InFlightOrDone.Reset();
		InFlightOrDone.AddZeroed(NumQWords);
		if (NumBits != NumQWords * 64)
		{
			uint32 Extras = NumQWords * 64 - NumBits;
			InFlightOrDone[NumQWords - 1] = (MAX_uint64 << (64 - Extras));
		}

		if (Pak.CacheBlocks[(int32)EBlockStatus::Complete] != IntervalTreeInvalidIndex)
		{
			Request.Status = EInRequestStatus::Complete;
			OverlappingNodesInIntervalTree<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this, &Pak, FirstByte, LastByte](TIntervalTreeIndex Index) -> bool
			{
				CacheBlockAllocator.Get(Index).InRequestRefCount++;
				MaskInterval(Index, CacheBlockAllocator, FirstByte, LastByte, Pak.BytesToBitsShift, &InFlightOrDone[0]);
				return true;
			}
			);
			for (uint32 Index = 0; Index < NumQWords; Index++)
			{
				if (InFlightOrDone[Index] != MAX_uint64)
				{
					Request.Status = EInRequestStatus::Waiting;
					break;
				}
			}
		}

		if (Request.Status == EInRequestStatus::Waiting)
		{
			if (Pak.CacheBlocks[(int32)EBlockStatus::InFlight] != IntervalTreeInvalidIndex)
			{
				Request.Status = EInRequestStatus::InFlight;
				OverlappingNodesInIntervalTree<FCacheBlock>(
					Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
					CacheBlockAllocator,
					FirstByte,
					LastByte,
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					[this, &Pak, FirstByte, LastByte](TIntervalTreeIndex Index) -> bool
				{
					CacheBlockAllocator.Get(Index).InRequestRefCount++;
					MaskInterval(Index, CacheBlockAllocator, FirstByte, LastByte, Pak.BytesToBitsShift, &InFlightOrDone[0]);
					return true;
				}
				);

				for (uint32 Index = 0; Index < NumQWords; Index++)
				{
					if (InFlightOrDone[Index] != MAX_uint64)
					{
						Request.Status = EInRequestStatus::Waiting;
						break;
					}
				}
			}
		}
		else
		{
#if PAK_EXTRA_CHECKS
			OverlappingNodesInIntervalTree<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this, &Pak, FirstByte, LastByte](TIntervalTreeIndex Index) -> bool
				{
					check(0); // if we are complete, then how come there are overlapping in flight blocks?
					return true;
				}
			);
#endif
		}
		{
			AddToIntervalTree<FPakInRequest>(
				&Pak.InRequests[Request.Priority][(int32)Request.Status],
				InRequestAllocator,
				NewIndex,
				Pak.StartShift,
				Pak.MaxShift
				);
		}
		check(&Request == &InRequestAllocator.Get(NewIndex));
		if (Request.Status == EInRequestStatus::Complete)
		{
			NotifyComplete(NewIndex);
			return true;
		}
		else if (Request.Status == EInRequestStatus::Waiting)
		{
			StartNextRequest();
		}
		return false;
	}

	void ClearBlock(FCacheBlock &Block)
	{
		UE_LOG(LogPakFile, Verbose, TEXT("FPakReadRequest[%016llX, %016llX) ClearBlock"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);

		if (Block.Memory)
		{
			check(Block.Size);
			BlockMemory -= Block.Size;
			DEC_MEMORY_STAT_BY(STAT_PakCacheMem, Block.Size);
			check(BlockMemory >= 0);

			FMemory::Free(Block.Memory);
			Block.Memory = nullptr;
		}
		Block.Next = IntervalTreeInvalidIndex;
		CacheBlockAllocator.Free(Block.Index);
	}

	void ClearRequest(FPakInRequest& DoneRequest)
	{
		uint64 Id = DoneRequest.UniqueID;
		TIntervalTreeIndex Index = DoneRequest.Index;

		DoneRequest.OffsetAndPakIndex = 0;
		DoneRequest.Size = 0;
		DoneRequest.Owner = nullptr;
		DoneRequest.UniqueID = 0;
		DoneRequest.Index = IntervalTreeInvalidIndex;
		DoneRequest.Next = IntervalTreeInvalidIndex;
		DoneRequest.Priority = AIOP_MIN;
		DoneRequest.Status = EInRequestStatus::Num;

		verify(OutstandingRequests.Remove(Id) == 1);
		InRequestAllocator.Free(Index);
	}
	void TrimCache(bool bDiscardAll = false)
	{
		// CachedFilesScopeLock is locked
		int32 NumToKeep = bDiscardAll ? 0 : GPakCache_NumUnreferencedBlocksToCache;
		int32 NumToRemove = FMath::Max<int32>(0, OffsetAndPakIndexOfSavedBlocked.Num() - NumToKeep);
		if (NumToRemove)
		{
			for (int32 Index = 0; Index < NumToRemove; Index++)
			{
				FJoinedOffsetAndPakIndex OffsetAndPakIndex = OffsetAndPakIndexOfSavedBlocked[Index];
				uint16 PakIndex = GetRequestPakIndex(OffsetAndPakIndex);
				int64 Offset = GetRequestOffset(OffsetAndPakIndex);
				FPakData& Pak = CachedPakData[PakIndex];
				MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
					&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
					CacheBlockAllocator,
					Offset,
					Offset,
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					[this](TIntervalTreeIndex BlockIndex) -> bool
				{
					FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
					if (!Block.InRequestRefCount)
					{
						UE_LOG(LogPakFile, Verbose, TEXT("FPakReadRequest[%016llX, %016llX) Discard Cached"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
						ClearBlock(Block);
						return true;
					}
					return false;
				}
				);


			}
			OffsetAndPakIndexOfSavedBlocked.RemoveAt(0, NumToRemove, false);
		}
	}

	void RemoveRequest(TIntervalTreeIndex Index)
	{
		// CachedFilesScopeLock is locked
		FPakInRequest& Request = InRequestAllocator.Get(Index);
		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		int64 Size = Request.Size;
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Request.Size > 0 && Request.Priority >= AIOP_MIN && Request.Priority <= AIOP_MAX && int32(Request.Status) >= 0 && int32(Request.Status) < int32(EInRequestStatus::Num));

		if (RemoveFromIntervalTree<FPakInRequest>(&Pak.InRequests[Request.Priority][(int32)Request.Status], InRequestAllocator, Index, Pak.StartShift, Pak.MaxShift))
		{

			int64 OffsetOfLastByte = Offset + Size - 1;
			MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
				&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				Offset,
				OffsetOfLastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this, OffsetOfLastByte](TIntervalTreeIndex BlockIndex) -> bool
				{
					FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
					check(Block.InRequestRefCount);
					if (!--Block.InRequestRefCount)
					{
						if (GPakCache_NumUnreferencedBlocksToCache && GetRequestOffset(Block.OffsetAndPakIndex) + Block.Size > OffsetOfLastByte) // last block
						{
							OffsetAndPakIndexOfSavedBlocked.Remove(Block.OffsetAndPakIndex);
							OffsetAndPakIndexOfSavedBlocked.Add(Block.OffsetAndPakIndex);
							return false;
						}
						ClearBlock(Block);
						return true;
					}
					return false;
				}
			);
			TrimCache();
			OverlappingNodesInIntervalTree<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				Offset,
				Offset + Size - 1,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this](TIntervalTreeIndex BlockIndex) -> bool
				{
					FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
					check(Block.InRequestRefCount);
					Block.InRequestRefCount--;
					return true;
				}
			);
		}
		else
		{
			check(0); // not found
		}
		ClearRequest(Request);
	}

	void NotifyComplete(TIntervalTreeIndex RequestIndex)
	{
		// CachedFilesScopeLock is locked
		FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);

		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Request.Size > 0 && Request.Priority >= AIOP_MIN && Request.Priority <= AIOP_MAX && Request.Status == EInRequestStatus::Complete);

		check(Request.Owner && Request.UniqueID);

		if (Request.Status == EInRequestStatus::Complete && Request.UniqueID == Request.Owner->UniqueID && RequestIndex == Request.Owner->InRequestIndex &&  Request.OffsetAndPakIndex == Request.Owner->OffsetAndPakIndex)
		{
			UE_LOG(LogPakFile, Verbose, TEXT("FPakReadRequest[%016llX, %016llX) Notify complete"), Request.OffsetAndPakIndex, Request.OffsetAndPakIndex + Request.Size);
			Request.Owner->RequestIsComplete();
			return;
		}
		else
		{
			check(0); // request should have been found
		}
	}

	FJoinedOffsetAndPakIndex GetNextBlock(EAsyncIOPriority& OutPriority)
	{
		bool bAcceptingPrecacheRequests = HasEnoughRoomForPrecache();

		// CachedFilesScopeLock is locked
		uint16 BestPakIndex = 0;
		FJoinedOffsetAndPakIndex BestNext = MAX_uint64;

		OutPriority = AIOP_MIN;
		bool bAnyOutstanding = false;
		for (EAsyncIOPriority Priority = AIOP_MAX;; Priority = EAsyncIOPriority(int32(Priority) - 1))
		{
			if (Priority == AIOP_Precache && !bAcceptingPrecacheRequests && bAnyOutstanding)
			{
				break;
			}
			for (int32 Pass = 0; ; Pass++)
			{
				FJoinedOffsetAndPakIndex LocalLastReadRequest = Pass ? 0 : LastReadRequest;

				uint16 PakIndex = GetRequestPakIndex(LocalLastReadRequest);
				int64 Offset = GetRequestOffset(LocalLastReadRequest);
				check(Offset <= CachedPakData[PakIndex].TotalSize);


				for (; BestNext == MAX_uint64 && PakIndex < CachedPakData.Num(); PakIndex++)
				{
					FPakData& Pak = CachedPakData[PakIndex];
					if (Pak.InRequests[Priority][(int32)EInRequestStatus::Complete] != IntervalTreeInvalidIndex)
					{
						bAnyOutstanding = true;
					}
					if (Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting] != IntervalTreeInvalidIndex)
					{
						uint64 Limit = uint64(Pak.TotalSize - 1);
						if (BestNext != MAX_uint64 && GetRequestPakIndex(BestNext) == PakIndex)
						{
							Limit = GetRequestOffset(BestNext) - 1;
						}

						OverlappingNodesInIntervalTreeWithShrinkingInterval<FPakInRequest>(
							Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
							InRequestAllocator,
							uint64(Offset),
							Limit,
							0,
							Pak.MaxNode,
							Pak.StartShift,
							Pak.MaxShift,
							[this, &Pak, &BestNext, &BestPakIndex, PakIndex, &Limit, LocalLastReadRequest](TIntervalTreeIndex Index) -> bool
							{
								FJoinedOffsetAndPakIndex First = FirstUnfilledBlockForRequest(Index, LocalLastReadRequest);
								check(LocalLastReadRequest != 0 || First != MAX_uint64); // if there was not trimming, and this thing is in the waiting list, then why was no start block found?
								if (First < BestNext)
								{
									BestNext = First;
									BestPakIndex = PakIndex;
									Limit = GetRequestOffset(BestNext) - 1;
								}
								return true; // always have to keep going because we want the smallest one
							}
						);
					}
				}
				if (!LocalLastReadRequest)
				{
					break; // this was a full pass
				}
			}

			if (Priority == AIOP_MIN || BestNext != MAX_uint64)
			{
				OutPriority = Priority;
				break;
			}
		}
		return BestNext;
	}

	bool AddNewBlock()
	{
		// CachedFilesScopeLock is locked
		EAsyncIOPriority RequestPriority;
		FJoinedOffsetAndPakIndex BestNext = GetNextBlock(RequestPriority);
		if (BestNext == MAX_uint64)
		{
			return false;
		}
		uint16 PakIndex = GetRequestPakIndex(BestNext);
		int64 Offset = GetRequestOffset(BestNext);
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset < Pak.TotalSize);
		int64 FirstByte = AlignDown(Offset, PAK_CACHE_GRANULARITY);
		int64 LastByte = FMath::Min(Align(FirstByte + (GPakCache_MaxRequestSizeToLowerLevelKB * 1024), PAK_CACHE_GRANULARITY) - 1, Pak.TotalSize - 1);
		check(FirstByte >= 0 && LastByte < Pak.TotalSize && LastByte >= 0 && LastByte >= FirstByte);

		uint32 NumBits = (PAK_CACHE_GRANULARITY + LastByte - FirstByte) / PAK_CACHE_GRANULARITY;
		uint32 NumQWords = (NumBits + 63) >> 6;

		static TArray<uint64> InFlightOrDone;
		InFlightOrDone.Reset();
		InFlightOrDone.AddZeroed(NumQWords);
		if (NumBits != NumQWords * 64)
		{
			uint32 Extras = NumQWords * 64 - NumBits;
			InFlightOrDone[NumQWords - 1] = (MAX_uint64 << (64 - Extras));
		}

		if (Pak.CacheBlocks[(int32)EBlockStatus::Complete] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}
		if (Pak.CacheBlocks[(int32)EBlockStatus::InFlight] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}

		static TArray<uint64> Requested;
		Requested.Reset();
		Requested.AddZeroed(NumQWords);
		for (EAsyncIOPriority Priority = AIOP_MAX;; Priority = EAsyncIOPriority(int32(Priority) - 1))
		{
			if (Priority + PAK_CACHE_MAX_PRIORITY_DIFFERENCE_MERGE < RequestPriority)
			{
				break;
			}
			if (Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting] != IntervalTreeInvalidIndex)
			{
				OverlappingNodesInIntervalTreeMask<FPakInRequest>(
					Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
					InRequestAllocator,
					FirstByte,
					LastByte,
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					Pak.BytesToBitsShift,
					&Requested[0]
					);
			}
			if (Priority == AIOP_MIN)
			{
				break;
			}
		}


		int64 Size = PAK_CACHE_GRANULARITY * 64 * NumQWords;
		for (uint32 Index = 0; Index < NumQWords; Index++)
		{
			uint64 NotAlreadyInFlightAndRequested = ((~InFlightOrDone[Index]) & Requested[Index]);
			if (NotAlreadyInFlightAndRequested != MAX_uint64)
			{
				Size = PAK_CACHE_GRANULARITY * 64 * Index;
				while (NotAlreadyInFlightAndRequested & 1)
				{
					Size += PAK_CACHE_GRANULARITY;
					NotAlreadyInFlightAndRequested >>= 1;
				}
				break;
			}
		}
		check(Size > 0 && Size <= (GPakCache_MaxRequestSizeToLowerLevelKB * 1024));
		Size = FMath::Min(FirstByte + Size, LastByte + 1) - FirstByte;

		TIntervalTreeIndex NewIndex = CacheBlockAllocator.Alloc();

		FCacheBlock& Block = CacheBlockAllocator.Get(NewIndex);
		Block.Index = NewIndex;
		Block.InRequestRefCount = 0;
		Block.Memory = nullptr;
		Block.OffsetAndPakIndex = MakeJoinedRequest(PakIndex, FirstByte);
		Block.Size = Size;
		Block.Status = EBlockStatus::InFlight;

		AddToIntervalTree<FCacheBlock>(
			&Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
			CacheBlockAllocator,
			NewIndex,
			Pak.StartShift,
			Pak.MaxShift
			);

		TArray<TIntervalTreeIndex> Inflights;

		for (EAsyncIOPriority Priority = AIOP_MAX;; Priority = EAsyncIOPriority(int32(Priority) - 1))
		{
			if (Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting] != IntervalTreeInvalidIndex)
			{
				MaybeRemoveOverlappingNodesInIntervalTree<FPakInRequest>(
					&Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
					InRequestAllocator,
					uint64(FirstByte),
					uint64(FirstByte + Size - 1),
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					[this, &Block, &Inflights](TIntervalTreeIndex RequestIndex) -> bool
				{
					Block.InRequestRefCount++;
					if (FirstUnfilledBlockForRequest(RequestIndex) == MAX_uint64)
					{
						InRequestAllocator.Get(RequestIndex).Next = IntervalTreeInvalidIndex;
						Inflights.Add(RequestIndex);
						return true;
					}
					return false;
				}
				);
			}
#if PAK_EXTRA_CHECKS
			OverlappingNodesInIntervalTree<FPakInRequest>(
				Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight],
				InRequestAllocator,
				uint64(FirstByte),
				uint64(FirstByte + Size - 1),
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[](TIntervalTreeIndex) -> bool
				{
					check(0); // if this is in flight, then why does it overlap my new block
					return false;
				}
			);
			OverlappingNodesInIntervalTree<FPakInRequest>(
				Pak.InRequests[Priority][(int32)EInRequestStatus::Complete],
				InRequestAllocator,
				uint64(FirstByte),
				uint64(FirstByte + Size - 1),
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[](TIntervalTreeIndex) -> bool
				{
					check(0); // if this is complete, then why does it overlap my new block
					return false;
				}
			);
#endif
			if (Priority == AIOP_MIN)
			{
				break;
			}
		}
		for (TIntervalTreeIndex Fli : Inflights)
		{
			FPakInRequest& CompReq = InRequestAllocator.Get(Fli);
			CompReq.Status = EInRequestStatus::InFlight;
			AddToIntervalTree(&Pak.InRequests[CompReq.Priority][(int32)EInRequestStatus::InFlight], InRequestAllocator, Fli, Pak.StartShift, Pak.MaxShift);
		}

		StartBlockTask(Block);
		return true;

	}

	int32 OpenTaskSlot()
	{
		int32 IndexToFill = -1;
		for (int32 Index = 0; Index < GPakCache_MaxRequestsToLowerLevel; Index++)
		{
			if (!RequestsToLower[Index].RequestHandle)
			{
				IndexToFill = Index;
				break;
			}
		}
		return IndexToFill;
	}


	bool HasRequestsAtStatus(EInRequestStatus Status)
	{
		for (uint16 PakIndex = 0; PakIndex < CachedPakData.Num(); PakIndex++)
		{
			FPakData& Pak = CachedPakData[PakIndex];
			for (EAsyncIOPriority Priority = AIOP_MAX;; Priority = EAsyncIOPriority(int32(Priority) - 1))
			{
				if (Pak.InRequests[Priority][(int32)Status] != IntervalTreeInvalidIndex)
				{
					return true;
				}
				if (Priority == AIOP_MIN)
				{
					break;
				}
			}
		}
		return false;
	}

	bool CanStartAnotherTask()
	{
		if (OpenTaskSlot() < 0)
		{
			return false;
		}
		return HasRequestsAtStatus(EInRequestStatus::Waiting);
	}
	void ClearOldBlockTasks()
	{
		if (!NotifyRecursion)
		{
			for (IAsyncReadRequest* Elem : RequestsToDelete)
			{
				Elem->WaitCompletion();
				delete Elem;
			}
			RequestsToDelete.Empty();
		}
	}
	void StartBlockTask(FCacheBlock& Block)
	{
		// CachedFilesScopeLock is locked

#define CHECK_REDUNDANT_READS (0)
#if CHECK_REDUNDANT_READS
		static struct FRedundantReadTracker
		{
			TMap<int64, double> LastReadTime;
			int32 NumRedundant;
			FRedundantReadTracker()
				: NumRedundant(0)
			{
			}

			void CheckBlock(int64 Offset, int64 Size)
			{
				double NowTime = FPlatformTime::Seconds();
				int64 StartBlock = Offset / PAK_CACHE_GRANULARITY;
				int64 LastBlock = (Offset + Size - 1) / PAK_CACHE_GRANULARITY;
				for (int64 CurBlock = StartBlock; CurBlock <= LastBlock; CurBlock++)
				{
					double LastTime = LastReadTime.FindRef(CurBlock);
					if (LastTime > 0.0 && NowTime - LastTime < 3.0)
					{
						NumRedundant++;
						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Redundant read at block %d, %6.1fms ago       (%d total redundant blocks)\r\n"), int32(CurBlock), 1000.0f * float(NowTime - LastTime), NumRedundant);
					}
					LastReadTime.Add(CurBlock, NowTime);
				}
			}
		} RedundantReadTracker;
#else
		static struct FRedundantReadTracker
		{
			FORCEINLINE void CheckBlock(int64 Offset, int64 Size)
			{
			}
		} RedundantReadTracker;

#endif

		int32 IndexToFill = OpenTaskSlot();
		if (IndexToFill < 0)
		{
			check(0);
			return;
		}
		EAsyncIOPriority Priority = AIOP_Normal; // the lower level requests are not prioritized at the moment
		check(Block.Status == EBlockStatus::InFlight);
		UE_LOG(LogPakFile, Verbose, TEXT("FPakReadRequest[%016llX, %016llX) StartBlockTask"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
		uint16 PakIndex = GetRequestPakIndex(Block.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		RequestsToLower[IndexToFill].BlockIndex = Block.Index;
		RequestsToLower[IndexToFill].RequestSize = Block.Size;
		RequestsToLower[IndexToFill].Memory = nullptr;
		check(&CacheBlockAllocator.Get(RequestsToLower[IndexToFill].BlockIndex) == &Block);

		FAsyncFileCallBack CallbackFromLower =
			[this, IndexToFill](bool bWasCanceled, IAsyncReadRequest* Request)
		{
			if (bSigned)
			{
				StartSignatureCheck(bWasCanceled, Request, IndexToFill);
			}
			else
			{
				NewRequestsToLowerComplete(bWasCanceled, Request, IndexToFill);
			}
		};

		RequestsToLower[IndexToFill].RequestHandle = Pak.Handle->ReadRequest(GetRequestOffset(Block.OffsetAndPakIndex), Block.Size, Priority, &CallbackFromLower);
		RedundantReadTracker.CheckBlock(GetRequestOffset(Block.OffsetAndPakIndex), Block.Size);
		LastReadRequest = Block.OffsetAndPakIndex + Block.Size;
		Loads++;
		LoadSize += Block.Size;
	}

	void CompleteRequest(bool bWasCanceled, uint8* Memory, TIntervalTreeIndex BlockIndex)
	{
		FCacheBlock& Block = CacheBlockAllocator.Get(BlockIndex);
		uint16 PakIndex = GetRequestPakIndex(Block.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Block.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		check(!Block.Memory && Block.Size);
		check(!bWasCanceled); // this is doable, but we need to transition requests back to waiting, inflight etc.

		if (!RemoveFromIntervalTree<FCacheBlock>(&Pak.CacheBlocks[(int32)EBlockStatus::InFlight], CacheBlockAllocator, Block.Index, Pak.StartShift, Pak.MaxShift))
		{
			check(0);
		}

		if (Block.InRequestRefCount == 0 || bWasCanceled)
		{
			check(Block.Size > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.Size);
			FMemory::Free(Memory);
			UE_LOG(LogPakFile, Verbose, TEXT("FPakReadRequest[%016llX, %016llX) Cancelled"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
			ClearBlock(Block);
		}
		else
		{
			Block.Memory = Memory;
			check(Block.Memory && Block.Size);
			BlockMemory += Block.Size;
			check(BlockMemory > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.Size);
			check(Block.Size > 0);
			INC_MEMORY_STAT_BY(STAT_PakCacheMem, Block.Size);

			if (BlockMemory > BlockMemoryHighWater)
			{
				BlockMemoryHighWater = BlockMemory;
				SET_MEMORY_STAT(STAT_PakCacheHighWater, BlockMemoryHighWater);

#if 0
				static int64 LastPrint = 0;
				if (BlockMemoryHighWater / 1024 / 1024 /16 != LastPrint)
				{
					LastPrint = BlockMemoryHighWater / 1024 / 1024 / 16;
					//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Precache HighWater %dMB\r\n"), int32(LastPrint));
					UE_LOG(LogPakFile, Log, TEXT("Precache HighWater %dMB\r\n"), int32(LastPrint * 16));
				}
#endif
			}
			Block.Status = EBlockStatus::Complete;
			AddToIntervalTree<FCacheBlock>(
				&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				Block.Index,
				Pak.StartShift,
				Pak.MaxShift
				);
			TArray<TIntervalTreeIndex> Completeds;
			for (EAsyncIOPriority Priority = AIOP_MAX;; Priority = EAsyncIOPriority(int32(Priority) - 1))
			{
				if (Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight] != IntervalTreeInvalidIndex)
				{
					MaybeRemoveOverlappingNodesInIntervalTree<FPakInRequest>(
						&Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight],
						InRequestAllocator,
						uint64(Offset),
						uint64(Offset + Block.Size - 1),
						0,
						Pak.MaxNode,
						Pak.StartShift,
						Pak.MaxShift,
						[this, &Completeds](TIntervalTreeIndex RequestIndex) -> bool
					{
						if (FirstUnfilledBlockForRequest(RequestIndex) == MAX_uint64)
						{
							InRequestAllocator.Get(RequestIndex).Next = IntervalTreeInvalidIndex;
							Completeds.Add(RequestIndex);
							return true;
						}
						return false;
					}
					);
				}
				if (Priority == AIOP_MIN)
				{
					break;
				}
			}
			for (TIntervalTreeIndex Comp : Completeds)
			{
				FPakInRequest& CompReq = InRequestAllocator.Get(Comp);
				CompReq.Status = EInRequestStatus::Complete;
				AddToIntervalTree(&Pak.InRequests[CompReq.Priority][(int32)EInRequestStatus::Complete], InRequestAllocator, Comp, Pak.StartShift, Pak.MaxShift);
				NotifyComplete(Comp); // potentially scary recursion here
			}
		}
	}

	bool StartNextRequest()
	{
		if (CanStartAnotherTask())
		{
			return AddNewBlock();
		}
		return false;
	}

	bool GetCompletedRequestData(FPakInRequest& DoneRequest, uint8* Result)
	{
		// CachedFilesScopeLock is locked
		check(DoneRequest.Status == EInRequestStatus::Complete);
		uint16 PakIndex = GetRequestPakIndex(DoneRequest.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(DoneRequest.OffsetAndPakIndex);
		int64 Size = DoneRequest.Size;

		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + DoneRequest.Size <= Pak.TotalSize && DoneRequest.Size > 0 && DoneRequest.Priority >= AIOP_MIN && DoneRequest.Priority <= AIOP_MAX && DoneRequest.Status == EInRequestStatus::Complete);

		int64 BytesCopied = 0;

#if 0 // this path removes the block in one pass, however, this is not what we want because it wrecks precaching, if we change back GetCompletedRequest needs to maybe start a new request and the logic of the IAsyncFile read needs to change
		MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
			&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
			CacheBlockAllocator,
			Offset,
			Offset + Size - 1,
			0,
			Pak.MaxNode,
			Pak.StartShift,
			Pak.MaxShift,
			[this, Offset, Size, &BytesCopied, Result, &Pak](TIntervalTreeIndex BlockIndex) -> bool
		{
			FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
			int64 BlockOffset = GetRequestOffset(Block.OffsetAndPakIndex);
			check(Block.Memory && Block.Size && BlockOffset >= 0 && BlockOffset + Block.Size <= Pak.TotalSize);

			int64 OverlapStart = FMath::Max(Offset, BlockOffset);
			int64 OverlapEnd = FMath::Min(Offset + Size, BlockOffset + Block.Size);
			check(OverlapEnd > OverlapStart);
			BytesCopied += OverlapEnd - OverlapStart;
			FMemory::Memcpy(Result + OverlapStart - Offset, Block.Memory + OverlapStart - BlockOffset, OverlapEnd - OverlapStart);
			check(Block.InRequestRefCount);
			if (!--Block.InRequestRefCount)
			{
				ClearBlock(Block);
				return true;
			}
			return false;
		}
		);

		if (!RemoveFromIntervalTree<FPakInRequest>(&Pak.InRequests[DoneRequest.Priority][(int32)EInRequestStatus::Complete], InRequestAllocator, DoneRequest.Index, Pak.StartShift, Pak.MaxShift))
		{
			check(0); // not found
		}
		ClearRequest(DoneRequest);
#else
		OverlappingNodesInIntervalTree<FCacheBlock>(
			Pak.CacheBlocks[(int32)EBlockStatus::Complete],
			CacheBlockAllocator,
			Offset,
			Offset + Size - 1,
			0,
			Pak.MaxNode,
			Pak.StartShift,
			Pak.MaxShift,
			[this, Offset, Size, &BytesCopied, Result, &Pak](TIntervalTreeIndex BlockIndex) -> bool
		{
			FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
			int64 BlockOffset = GetRequestOffset(Block.OffsetAndPakIndex);
			check(Block.Memory && Block.Size && BlockOffset >= 0 && BlockOffset + Block.Size <= Pak.TotalSize);

			int64 OverlapStart = FMath::Max(Offset, BlockOffset);
			int64 OverlapEnd = FMath::Min(Offset + Size, BlockOffset + Block.Size);
			check(OverlapEnd > OverlapStart);
			BytesCopied += OverlapEnd - OverlapStart;
			FMemory::Memcpy(Result + OverlapStart - Offset, Block.Memory + OverlapStart - BlockOffset, OverlapEnd - OverlapStart);
			return true;
		}
		);
#endif
		check(BytesCopied == Size);


		return true;
	}

	///// Below here are the thread entrypoints

public:

	void NewRequestsToLowerComplete(bool bWasCanceled, IAsyncReadRequest* Request, int32 Index)
	{
		FScopeLock Lock(&CachedFilesScopeLock);
		RequestsToLower[Index].RequestHandle = Request;
		ClearOldBlockTasks();
		NotifyRecursion++;
		if (!RequestsToLower[Index].Memory) // might have already been filled in by the signature check
		{
			RequestsToLower[Index].Memory = Request->GetReadResults();
		}
		CompleteRequest(bWasCanceled, RequestsToLower[Index].Memory, RequestsToLower[Index].BlockIndex);
		RequestsToLower[Index].RequestHandle = nullptr;
		RequestsToDelete.Add(Request);
		RequestsToLower[Index].BlockIndex = IntervalTreeInvalidIndex;
		StartNextRequest();
		NotifyRecursion--;
	}

	bool QueueRequest(IPakRequestor* Owner, FName File, int64 PakFileSize, int64 Offset, int64 Size, EAsyncIOPriority Priority)
	{
		check(Owner && File != NAME_None && Size > 0 && Offset >= 0 && Offset < PakFileSize && Priority >= AIOP_MIN && Priority <= AIOP_MAX);
		FScopeLock Lock(&CachedFilesScopeLock);
		uint16* PakIndexPtr = RegisterPakFile(File, PakFileSize);
		if (PakIndexPtr == nullptr)
		{
			return false;
		}
		uint16 PakIndex = *PakIndexPtr;
		FPakData& Pak = CachedPakData[PakIndex];
		check(Pak.Name == File && Pak.TotalSize == PakFileSize && Pak.Handle);

		TIntervalTreeIndex RequestIndex = InRequestAllocator.Alloc();
		FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);
		FJoinedOffsetAndPakIndex RequestOffsetAndPakIndex = MakeJoinedRequest(PakIndex, Offset);
		Request.OffsetAndPakIndex = RequestOffsetAndPakIndex;
		Request.Size = Size;
		Request.Priority = Priority;
		Request.Status = EInRequestStatus::Waiting;
		Request.Owner = Owner;
		Request.UniqueID = NextUniqueID++;
		Request.Index = RequestIndex;
		check(Request.Next == IntervalTreeInvalidIndex);
		Owner->OffsetAndPakIndex = Request.OffsetAndPakIndex;
		Owner->UniqueID = Request.UniqueID;
		Owner->InRequestIndex = RequestIndex;
		check(!OutstandingRequests.Contains(Request.UniqueID));
		OutstandingRequests.Add(Request.UniqueID, RequestIndex);
		if (AddRequest(RequestIndex))
		{
			UE_LOG(LogPakFile, Verbose, TEXT("FPakReadRequest[%016llX, %016llX) QueueRequest HOT"), RequestOffsetAndPakIndex, RequestOffsetAndPakIndex + Request.Size);
		}
		else
		{
			UE_LOG(LogPakFile, Verbose, TEXT("FPakReadRequest[%016llX, %016llX) QueueRequest COLD"), RequestOffsetAndPakIndex, RequestOffsetAndPakIndex + Request.Size);
		}

		return true;
	}

	bool GetCompletedRequest(IPakRequestor* Owner, uint8* UserSuppliedMemory)
	{
		check(Owner);
		FScopeLock Lock(&CachedFilesScopeLock);
		ClearOldBlockTasks();
		TIntervalTreeIndex RequestIndex = OutstandingRequests.FindRef(Owner->UniqueID);
		static_assert(IntervalTreeInvalidIndex == 0, "FindRef will return 0 for something not found"); 
		if (RequestIndex)
		{
			FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);
			check(Owner == Request.Owner && Request.Status == EInRequestStatus::Complete && Request.UniqueID == Request.Owner->UniqueID && RequestIndex == Request.Owner->InRequestIndex &&  Request.OffsetAndPakIndex == Request.Owner->OffsetAndPakIndex);
			return GetCompletedRequestData(Request, UserSuppliedMemory);
		}
		return false; // canceled
	}

	void CancelRequest(IPakRequestor* Owner)
	{
		check(Owner);
		FScopeLock Lock(&CachedFilesScopeLock);
		ClearOldBlockTasks();
		TIntervalTreeIndex RequestIndex = OutstandingRequests.FindRef(Owner->UniqueID);
		static_assert(IntervalTreeInvalidIndex == 0, "FindRef will return 0 for something not found");
		if (RequestIndex)
		{
			FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);
			check(Owner == Request.Owner && Request.UniqueID == Request.Owner->UniqueID && RequestIndex == Request.Owner->InRequestIndex &&  Request.OffsetAndPakIndex == Request.Owner->OffsetAndPakIndex);
			RemoveRequest(RequestIndex);
		}
		StartNextRequest();
	}

	bool IsProbablyIdle() // nothing to prevent new requests from being made before I return
	{
		FScopeLock Lock(&CachedFilesScopeLock);
		return !HasRequestsAtStatus(EInRequestStatus::Waiting) && !HasRequestsAtStatus(EInRequestStatus::InFlight);
	}

	void Unmount(FName PakFile)
	{
		FScopeLock Lock(&CachedFilesScopeLock);
		uint16* PakIndexPtr = CachedPaks.Find(PakFile);
		if (!PakIndexPtr)
		{
			UE_LOG(LogPakFile, Log, TEXT("Pak file %s was never used, so nothing to unmount"), *PakFile.ToString());
			return; // never used for anything, nothing to check or clean up
		}
		TrimCache(true);
		uint16 PakIndex = *PakIndexPtr;
		FPakData& Pak = CachedPakData[PakIndex];
		int64 Offset = MakeJoinedRequest(PakIndex, 0);

		bool bHasOutstandingRequests = false;

		OverlappingNodesInIntervalTree<FCacheBlock>(
			Pak.CacheBlocks[(int32)EBlockStatus::Complete],
			CacheBlockAllocator,
			0,
			Offset + Pak.TotalSize - 1,
			0,
			Pak.MaxNode,
			Pak.StartShift,
			Pak.MaxShift,
			[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
		{
			check(!"Pak cannot be unmounted with outstanding requests");
			bHasOutstandingRequests = true;
			return false;
		}
		);
		OverlappingNodesInIntervalTree<FCacheBlock>(
			Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
			CacheBlockAllocator,
			0,
			Offset + Pak.TotalSize - 1,
			0,
			Pak.MaxNode,
			Pak.StartShift,
			Pak.MaxShift,
			[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
		{
			check(!"Pak cannot be unmounted with outstanding requests");
			bHasOutstandingRequests = true;
			return false;
		}
		);
		for (EAsyncIOPriority Priority = AIOP_MAX;; Priority = EAsyncIOPriority(int32(Priority) - 1))
		{
			OverlappingNodesInIntervalTree<FPakInRequest>(
				Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight],
				InRequestAllocator,
				0,
				Offset + Pak.TotalSize - 1,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
			{
				check(!"Pak cannot be unmounted with outstanding requests");
				bHasOutstandingRequests = true;
				return false;
			}
			);
			OverlappingNodesInIntervalTree<FPakInRequest>(
				Pak.InRequests[Priority][(int32)EInRequestStatus::Complete],
				InRequestAllocator,
				0,
				Offset + Pak.TotalSize - 1,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
			{
				check(!"Pak cannot be unmounted with outstanding requests");
				bHasOutstandingRequests = true;
				return false;
			}
			);
			OverlappingNodesInIntervalTree<FPakInRequest>(
				Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
				InRequestAllocator,
				0,
				Offset + Pak.TotalSize - 1,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
			{
				check(!"Pak cannot be unmounted with outstanding requests");
				bHasOutstandingRequests = true;
				return false;
			}
			);
			if (Priority == AIOP_MIN)
			{
				break;
			}
		}
		if (!bHasOutstandingRequests)
		{
			UE_LOG(LogPakFile, Log, TEXT("Pak file %s removed from pak precacher."), *PakFile.ToString());
			CachedPaks.Remove(PakFile);
			check(Pak.Handle);
			delete Pak.Handle;
			Pak.Handle = nullptr;
			int32 NumToTrim = 0;
			for (int32 Index = CachedPakData.Num() - 1; Index >= 0; Index--)
			{
				if (!CachedPakData[Index].Handle)
				{
					NumToTrim++;
				}
				else
				{
					break;
				}
			}
			if (NumToTrim)
			{
				CachedPakData.RemoveAt(CachedPakData.Num() - NumToTrim, NumToTrim);
			}
		}
		else
		{
			UE_LOG(LogPakFile, Log, TEXT("Pak file %s was NOT removed from pak precacher because it had outstanding requests."), *PakFile.ToString());
		}
	}


	// these are not threadsafe and should only be used for synthetic testing
	uint64 GetLoadSize()
	{
		return LoadSize;
	}
	uint32 GetLoads()
	{
		return Loads;
	}
	uint32 GetFrees()
	{
		return Frees;
	}

	void DumpBlocks()
	{
		while (!FPakPrecacher::Get().IsProbablyIdle())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitDumpBlocks);
			FPlatformProcess::SleepNoStats(0.001f);
		}
		FScopeLock Lock(&CachedFilesScopeLock);
		bool bDone = !HasRequestsAtStatus(EInRequestStatus::Waiting) && !HasRequestsAtStatus(EInRequestStatus::InFlight) && !HasRequestsAtStatus(EInRequestStatus::Complete);

		if (!bDone)
		{
			UE_LOG(LogPakFile, Log, TEXT("PakCache has outstanding requests with %llu total memory."), BlockMemory);
		}
		else
		{
			UE_LOG(LogPakFile, Log, TEXT("PakCache has no outstanding requests with %llu total memory."), BlockMemory);
		}
	}
};

static void WaitPrecache(const TArray<FString>& Args)
{
	uint32 Frees = FPakPrecacher::Get().GetFrees();
	uint32 Loads = FPakPrecacher::Get().GetLoads();
	uint64 LoadSize = FPakPrecacher::Get().GetLoadSize();

	double StartTime = FPlatformTime::Seconds();

	while (!FPakPrecacher::Get().IsProbablyIdle())
	{
		check(Frees == FPakPrecacher::Get().GetFrees()); // otherwise we are discarding things, which is not what we want for this synthetic test
		QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitPrecache);
		FPlatformProcess::SleepNoStats(0.001f);
	}
	Loads = FPakPrecacher::Get().GetLoads() - Loads;
	LoadSize = FPakPrecacher::Get().GetLoadSize() - LoadSize;
	float TimeSpent = FPlatformTime::Seconds() - StartTime;
	float LoadSizeMB = float(LoadSize) / (1024.0f * 1024.0f);
	float MBs = LoadSizeMB / TimeSpent;
	UE_LOG(LogPakFile, Log, TEXT("Loaded %4d blocks (align %4dKB) totalling %7.2fMB in %4.2fs   = %6.2fMB/s"), Loads, PAK_CACHE_GRANULARITY / 1024, LoadSizeMB, TimeSpent, MBs);
}

static FAutoConsoleCommand WaitPrecacheCmd(
	TEXT("pak.WaitPrecache"),
	TEXT("Debug command to wait on the pak precache."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&WaitPrecache)
	);

static void DumpBlocks(const TArray<FString>& Args)
{
	FPakPrecacher::Get().DumpBlocks();
}

static FAutoConsoleCommand DumpBlocksCmd(
	TEXT("pak.DumpBlocks"),
	TEXT("Debug command to spew the outstanding blocks."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpBlocks)
	);

static FCriticalSection FPakReadRequestEvent;

class FPakAsyncReadFileHandle;

struct FCachedAsyncBlock
{
	class FPakReadRequest* RawRequest;
	uint8* Raw; // compressed, encrypted and/or signature not checked
	uint8* Processed; // decompressed, deencrypted and signature checked
	FGraphEventRef CPUWorkGraphEvent;
	int32 RawSize;
	int32 ProcessedSize;
	int32 RefCount;
	int32 BlockIndex;
	bool bInFlight;
	bool bCPUWorkIsComplete;
	bool bCancelledBlock;
	FCachedAsyncBlock()
		: RawRequest(0)
		, Raw(nullptr)
		, Processed(nullptr)
		, RawSize(0)
		, ProcessedSize(0)
		, RefCount(0)
		, BlockIndex(-1)
		, bInFlight(false)
		, bCPUWorkIsComplete(false)
		, bCancelledBlock(false)
	{
	}
};


class FPakReadRequestBase : public IAsyncReadRequest, public IPakRequestor
{
protected:

	int64 Offset;
	int64 BytesToRead;
	FEvent* WaitEvent;
	FCachedAsyncBlock* BlockPtr;
	EAsyncIOPriority Priority;
	bool bRequestOutstanding;
	bool bNeedsRemoval;
	bool bInternalRequest; // we are using this internally to deal with compressed, encrypted and signed, so we want the memory back from a precache request.

public:
	FPakReadRequestBase(FName InPakFile, int64 PakFileSize, FAsyncFileCallBack* CompleteCallback, int64 InOffset, int64 InBytesToRead, EAsyncIOPriority InPriority, uint8* UserSuppliedMemory, bool bInInternalRequest = false, FCachedAsyncBlock* InBlockPtr = nullptr)
		: IAsyncReadRequest(CompleteCallback, false, UserSuppliedMemory)
		, Offset(InOffset)
		, BytesToRead(InBytesToRead)
		, WaitEvent(nullptr)
		, BlockPtr(InBlockPtr)
		, Priority(InPriority)
		, bRequestOutstanding(true)
		, bNeedsRemoval(true)
		, bInternalRequest(bInInternalRequest)
	{
	}

	virtual ~FPakReadRequestBase()
	{
		if (bNeedsRemoval)
		{
			FPakPrecacher::Get().CancelRequest(this);
		}
		if (Memory && !bUserSuppliedMemory)
		{
			// this can happen with a race on cancel, it is ok, they didn't take the memory, free it now
			check(BytesToRead > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
			FMemory::Free(Memory);
		}
		Memory = nullptr;
	}

	// IAsyncReadRequest Interface

	virtual void WaitCompletionImpl(float TimeLimitSeconds) override
	{
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			if (bRequestOutstanding)
			{
				check(!WaitEvent);
				WaitEvent = FPlatformProcess::GetSynchEventFromPool(true);
			}
		}
		if (WaitEvent)
		{
			if (TimeLimitSeconds == 0.0f)
			{
				WaitEvent->Wait();
				check(!bRequestOutstanding);
			}
			else
			{
				WaitEvent->Wait(TimeLimitSeconds * 1000.0f);
			}
			FScopeLock Lock(&FPakReadRequestEvent);
			FPlatformProcess::ReturnSynchEventToPool(WaitEvent);
			WaitEvent = nullptr;
		}
	}
	virtual void CancelImpl() override
	{
		check(!WaitEvent); // you canceled from a different thread that you waited from
		FPakPrecacher::Get().CancelRequest(this);
		bNeedsRemoval = false;
		if (bRequestOutstanding)
		{
			bRequestOutstanding = false;
			SetComplete();
		}
	}

	FCachedAsyncBlock& GetBlock()
	{
		check(bInternalRequest && BlockPtr);
		return *BlockPtr;
	}
};

class FPakReadRequest : public FPakReadRequestBase
{
public:

	FPakReadRequest(FName InPakFile, int64 PakFileSize, FAsyncFileCallBack* CompleteCallback, int64 InOffset, int64 InBytesToRead, EAsyncIOPriority InPriority, uint8* UserSuppliedMemory, bool bInInternalRequest = false, FCachedAsyncBlock* InBlockPtr = nullptr)
		: FPakReadRequestBase(InPakFile, PakFileSize, CompleteCallback, InOffset, InBytesToRead, InPriority, UserSuppliedMemory, bInInternalRequest, InBlockPtr)
	{
		check(Offset >= 0 && BytesToRead > 0);
		check(bInternalRequest || Priority > AIOP_Precache || !bUserSuppliedMemory); // you never get bits back from a precache request, so why supply memory?

		if (!FPakPrecacher::Get().QueueRequest(this, InPakFile, PakFileSize, Offset, BytesToRead, Priority))
		{
			bRequestOutstanding = false;
			SetComplete();
		}
	}

	virtual void RequestIsComplete() override
	{
		check(bRequestOutstanding);
		if (!bCanceled && (bInternalRequest || Priority > AIOP_Precache))
		{
			if (!bUserSuppliedMemory)
			{
				check(!Memory);
				Memory = (uint8*)FMemory::Malloc(BytesToRead);
				check(BytesToRead > 0);
				INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
			}
			else
			{
				check(Memory);
			}
			if (!FPakPrecacher::Get().GetCompletedRequest(this, Memory))
			{
				check(bCanceled);
			}
		}
		SetDataComplete();
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			bRequestOutstanding = false;
			if (WaitEvent)
			{
				WaitEvent->Trigger();
			}
			SetAllComplete();
		}
	}
};

class FPakEncryptedReadRequest : public FPakReadRequestBase
{
	int64 OriginalOffset;
	int64 OriginalSize;

public:

	FPakEncryptedReadRequest(FName InPakFile, int64 PakFileSize, FAsyncFileCallBack* CompleteCallback, int64 InPakFileStartOffset, int64 InFileOffset, int64 InBytesToRead, EAsyncIOPriority InPriority, uint8* UserSuppliedMemory, bool bInInternalRequest = false, FCachedAsyncBlock* InBlockPtr = nullptr)
		: FPakReadRequestBase(InPakFile, PakFileSize, CompleteCallback, InPakFileStartOffset + InFileOffset, InBytesToRead, InPriority, UserSuppliedMemory, bInInternalRequest, InBlockPtr)
		, OriginalOffset(InPakFileStartOffset + InFileOffset)
		, OriginalSize(InBytesToRead)
	{
		Offset = InPakFileStartOffset + AlignDown(InFileOffset, FAES::AESBlockSize);
		BytesToRead = Align(InFileOffset + InBytesToRead, FAES::AESBlockSize) - AlignDown(InFileOffset, FAES::AESBlockSize);

		if (!FPakPrecacher::Get().QueueRequest(this, InPakFile, PakFileSize, Offset, BytesToRead, Priority))
		{
			bRequestOutstanding = false;
			SetComplete();
		}
	}

	virtual void RequestIsComplete() override
	{
		check(bRequestOutstanding);
		if (!bCanceled && (bInternalRequest || Priority > AIOP_Precache))
		{
			uint8* OversizedBuffer = nullptr;
			if (OriginalOffset != Offset || OriginalSize != BytesToRead)
			{
				// We've read some bytes from before the requested offset, so we need to grab that larger amount
				// from read request and then cut out the bit we want!
				OversizedBuffer = (uint8*)FMemory::Malloc(BytesToRead);
			}
			uint8* DestBuffer = Memory;

			if (!bUserSuppliedMemory)
			{
				check(!Memory);
				DestBuffer = (uint8*)FMemory::Malloc(OriginalSize);
				INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, OriginalSize);
			}
			else
			{
				check(DestBuffer);
			}

			if (!FPakPrecacher::Get().GetCompletedRequest(this, OversizedBuffer != nullptr ? OversizedBuffer : DestBuffer))
			{
				check(bCanceled);
				if (!bUserSuppliedMemory)
				{
					check(!Memory && DestBuffer);
					FMemory::Free(DestBuffer);
					DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, OriginalSize);
					DestBuffer = nullptr;
				}
				if (OversizedBuffer)
				{
					FMemory::Free(OversizedBuffer);
					OversizedBuffer = nullptr;
				}
			}
			else
			{
				Memory = DestBuffer;
				check(Memory);
				INC_DWORD_STAT(STAT_PakCache_UncompressedDecrypts);

				if (OversizedBuffer)
				{
					check(IsAligned((void*)BytesToRead, FAES::AESBlockSize));
					DecryptData(OversizedBuffer, BytesToRead);
					FMemory::Memcpy(Memory, OversizedBuffer + (OriginalOffset - Offset), OriginalSize);
					FMemory::Free(OversizedBuffer);
				}
				else
				{
					DecryptData(Memory, Align(OriginalSize, FAES::AESBlockSize));
				}
			}
		}
		SetDataComplete();
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			bRequestOutstanding = false;
			if (WaitEvent)
			{
				WaitEvent->Trigger();
			}
			SetAllComplete();
		}
	}
};

class FPakSizeRequest : public IAsyncReadRequest
{
public:
	FPakSizeRequest(FAsyncFileCallBack* CompleteCallback, int64 InFileSize)
		: IAsyncReadRequest(CompleteCallback, true, nullptr)
	{
		Size = InFileSize;
		SetComplete();
	}
	virtual void WaitCompletionImpl(float TimeLimitSeconds) override	
	{
	}
	virtual void CancelImpl()
	{
	}
};

class FPakProcessedReadRequest : public IAsyncReadRequest
{
	FPakAsyncReadFileHandle* Owner;
	int64 Offset;
	int64 BytesToRead;
	FEvent* WaitEvent;
	FThreadSafeCounter CompleteRace; // this is used to resolve races with natural completion and cancel; there can be only one.
	EAsyncIOPriority Priority;
	bool bRequestOutstanding;
	bool bHasCancelled;
	bool bHasCompleted;

	TSet<FCachedAsyncBlock*> MyCanceledBlocks;

public:
	FPakProcessedReadRequest(FPakAsyncReadFileHandle* InOwner, FAsyncFileCallBack* CompleteCallback, int64 InOffset, int64 InBytesToRead, EAsyncIOPriority InPriority, uint8* UserSuppliedMemory)
		: IAsyncReadRequest(CompleteCallback, false, UserSuppliedMemory)
		, Owner(InOwner)
		, Offset(InOffset)
		, BytesToRead(InBytesToRead)
		, WaitEvent(nullptr)
		, Priority(InPriority)
		, bRequestOutstanding(true)
		, bHasCancelled(false)
		, bHasCompleted(false)
	{
		check(Offset >= 0 && BytesToRead > 0);
		check(Priority > AIOP_Precache || !bUserSuppliedMemory); // you never get bits back from a precache request, so why supply memory?
	}

	virtual ~FPakProcessedReadRequest()
	{
		check(!MyCanceledBlocks.Num());
		if (!bHasCancelled)
		{
			DoneWithRawRequests();
		}
		if (Memory && !bUserSuppliedMemory)
		{
			// this can happen with a race on cancel, it is ok, they didn't take the memory, free it now
			check(BytesToRead > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
			FMemory::Free(Memory);
		}
		Memory = nullptr;
	}

	bool WasCanceled()
	{
		return bHasCancelled;
	}

	virtual void WaitCompletionImpl(float TimeLimitSeconds) override
	{
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			if (bRequestOutstanding)
			{
				check(!WaitEvent);
				WaitEvent = FPlatformProcess::GetSynchEventFromPool(true);
			}
		}
		if (WaitEvent)
		{
			if (TimeLimitSeconds == 0.0f)
			{
				WaitEvent->Wait();
				check(!bRequestOutstanding);
			}
			else
			{
				WaitEvent->Wait(TimeLimitSeconds * 1000.0f);
			}
			FScopeLock Lock(&FPakReadRequestEvent);
			FPlatformProcess::ReturnSynchEventToPool(WaitEvent);
			WaitEvent = nullptr;
		}
	}
	virtual void CancelImpl() override
	{
		check(!WaitEvent); // you canceled from a different thread that you waited from
		if (CompleteRace.Increment() == 1)
		{
			if (bRequestOutstanding)
			{
				CancelRawRequests();
				if (!MyCanceledBlocks.Num())
				{
					bRequestOutstanding = false;
					SetComplete();
				}
			}
		}
	}

	void RequestIsComplete()
	{
		if (CompleteRace.Increment() == 1)
		{
			check(bRequestOutstanding);
			if (!bCanceled && Priority > AIOP_Precache)
			{
				GatherResults();
			}
			SetDataComplete();
			{
				FScopeLock Lock(&FPakReadRequestEvent);
				bRequestOutstanding = false;
				if (WaitEvent)
				{
					WaitEvent->Trigger();
				}
				SetAllComplete();
			}
		}
	}
	bool CancelBlockComplete(FCachedAsyncBlock* BlockPtr)
	{
		check(MyCanceledBlocks.Contains(BlockPtr));
		MyCanceledBlocks.Remove(BlockPtr);
		if (!MyCanceledBlocks.Num())
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			bRequestOutstanding = false;
			if (WaitEvent)
			{
				WaitEvent->Trigger();
			}
			SetComplete();
			return true;
		}
		return false;
	}


	void GatherResults();
	void DoneWithRawRequests();
	bool CheckCompletion(const FPakEntry& FileEntry, int32 BlockIndex, TArray<FCachedAsyncBlock*>& Blocks);
	void CancelRawRequests();
};

FAutoConsoleTaskPriority CPrio_AsyncIOCPUWorkTaskPriority(
	TEXT("TaskGraph.TaskPriorities.AsyncIOCPUWork"),
	TEXT("Task and thread priority for decompression, decryption and signature checking of async IO from a pak file."),
	ENamedThreads::BackgroundThreadPriority, // if we have background priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::NormalTaskPriority // if we don't have background threads, then use normal priority threads at normal task priority instead
	);

class FAsyncIOCPUWorkTask
{
	FPakAsyncReadFileHandle& Owner;
	FCachedAsyncBlock* BlockPtr;

public:
	FORCEINLINE FAsyncIOCPUWorkTask(FPakAsyncReadFileHandle& InOwner, FCachedAsyncBlock* InBlockPtr)
		: Owner(InOwner)
		, BlockPtr(InBlockPtr)
	{
	}
	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FsyncIOCPUWorkTask, STATGROUP_TaskGraphTasks);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_AsyncIOCPUWorkTaskPriority.Get();
	}
	FORCEINLINE static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
};

class FAsyncIOSignatureCheckTask
{
	bool bWasCanceled;
	IAsyncReadRequest* Request;
	int32 IndexToFill;

public:
	FORCEINLINE FAsyncIOSignatureCheckTask(bool bInWasCanceled, IAsyncReadRequest* InRequest, int32 InIndexToFill)
		: bWasCanceled(bInWasCanceled)
		, Request(InRequest)
		, IndexToFill(InIndexToFill)
	{
	}

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FsyncIOCPUWorkTask, STATGROUP_TaskGraphTasks);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_AsyncIOCPUWorkTaskPriority.Get();
	}
	FORCEINLINE static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FPakPrecacher::Get().DoSignatureCheck(bWasCanceled, Request, IndexToFill);
	}
};

void FPakPrecacher::StartSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 Index)
{
	TGraphTask<FAsyncIOSignatureCheckTask>::CreateTask().ConstructAndDispatchWhenReady(bWasCanceled, Request, Index);
}

void FPakPrecacher::DoSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 Index)
{
	int64 SignatureIndex = -1;
	int64 NumSignaturesToCheck = 0;
	const uint8* Data = nullptr;
	int64 RequestSize = 0;
	int64 RequestOffset = 0;
	uint16 PakIndex;
	TPakChunkHash MasterSignatureHash = 0;

	{
		// Try and keep lock for as short a time as possible. Find our request and copy out the data we need
		FScopeLock Lock(&CachedFilesScopeLock);
		FRequestToLower& RequestToLower = RequestsToLower[Index];
		RequestToLower.RequestHandle = Request;
		RequestToLower.Memory = Request->GetReadResults();

		NumSignaturesToCheck = Align(RequestToLower.RequestSize, FPakInfo::MaxChunkDataSize) / FPakInfo::MaxChunkDataSize;
		check(NumSignaturesToCheck >= 1);

		FCacheBlock& Block = CacheBlockAllocator.Get(RequestToLower.BlockIndex);
		RequestOffset = GetRequestOffset(Block.OffsetAndPakIndex);
		check((RequestOffset % FPakInfo::MaxChunkDataSize) == 0);
		RequestSize = RequestToLower.RequestSize;
		PakIndex = GetRequestPakIndex(Block.OffsetAndPakIndex);
		Data = RequestToLower.Memory;
		SignatureIndex = RequestOffset / FPakInfo::MaxChunkDataSize;

		MasterSignatureHash = CachedPakData[PakIndex].OriginalSignatureFileHash;
	}

	check(Data);
	check(NumSignaturesToCheck > 0);
	check(RequestSize > 0);
	check(RequestOffset >= 0);

	// Hash the contents of the incoming buffer and check that it matches what we expected
	for (int64 SignedChunkIndex = 0; SignedChunkIndex < NumSignaturesToCheck; ++SignedChunkIndex, ++SignatureIndex)
	{
		int64 Size = FMath::Min(RequestSize, (int64)FPakInfo::MaxChunkDataSize);

		{
			SCOPE_SECONDS_ACCUMULATOR(STAT_PakCache_SigningChunkHashTime);

			TPakChunkHash ThisHash = ComputePakChunkHash(Data, Size);
			bool bChunkHashesMatch;
			{
				FScopeLock Lock(&CachedFilesScopeLock);
				FPakData* PakData = &CachedPakData[PakIndex];
				bChunkHashesMatch = ThisHash == PakData->ChunkHashes[SignatureIndex];
			}

			if (!bChunkHashesMatch)
			{
				FScopeLock Lock(&CachedFilesScopeLock);
				FPakData* PakData = &CachedPakData[PakIndex];

				UE_LOG(LogPakFile, Warning, TEXT("Pak chunk signing mismatch on chunk [%i/%i]! Expected 0x%8X, Received 0x%8X"), SignatureIndex, PakData->ChunkHashes.Num(), PakData->OriginalSignatureFileHash, ThisHash);
				UE_LOG(LogPakFile, Warning, TEXT("Pak file has been corrupted or tampered with!"));

				// Check the signatures are still as we expected them
				TPakChunkHash CurrentSignatureHash = ComputePakChunkHash(&PakData->ChunkHashes[0], PakData->ChunkHashes.Num() * sizeof(TPakChunkHash));
				if (PakData->OriginalSignatureFileHash != CurrentSignatureHash)
				{
					UE_LOG(LogPakFile, Warning, TEXT("Master signature table has changed since initialization!"));
				}

				ensure(bChunkHashesMatch);

#if PAK_SIGNATURE_CHECK_FAILS_ARE_FATAL
				FPlatformMisc::RequestExit(true);
#endif
			}
		}

		INC_MEMORY_STAT_BY(STAT_PakCache_SigningChunkHashSize, Size);

		RequestOffset += Size;
		Data += Size;
		RequestSize -= Size;
	}

	NewRequestsToLowerComplete(bWasCanceled, Request, Index);
}

class FPakAsyncReadFileHandle final : public IAsyncReadFileHandle
{
	FName PakFile;
	int64 PakFileSize;
	int64 OffsetInPak;
	int64 CompressedFileSize;
	int64 UncompressedFileSize;
	const FPakEntry* FileEntry;
	TSet<FPakProcessedReadRequest*> LiveRequests;
	TArray<FCachedAsyncBlock*> Blocks;
	FAsyncFileCallBack ReadCallbackFunction;
	FCriticalSection CriticalSection;
	int32 NumLiveRawRequests;

	TMap<FCachedAsyncBlock*, FPakProcessedReadRequest*> OutstandingCancelMapBlock;

	FCachedAsyncBlock& GetBlock(int32 Index)
	{
		if (!Blocks[Index])
		{
			Blocks[Index] = new FCachedAsyncBlock;
			Blocks[Index]->BlockIndex = Index;
		}
		return *Blocks[Index];
	}


public:
	FPakAsyncReadFileHandle(const FPakEntry* InFileEntry, FPakFile* InPakFile, const TCHAR* Filename)
		: PakFile(InPakFile->GetFilenameName())
		, PakFileSize(InPakFile->TotalSize())
		, FileEntry(InFileEntry)
		, NumLiveRawRequests(0)
	{
		OffsetInPak = FileEntry->Offset + FileEntry->GetSerializedSize(InPakFile->GetInfo().Version);
		UncompressedFileSize = FileEntry->UncompressedSize;
		CompressedFileSize = FileEntry->UncompressedSize;
		if (FileEntry->CompressionMethod != COMPRESS_None && UncompressedFileSize)
		{
			check(FileEntry->CompressionBlocks.Num());
			CompressedFileSize = FileEntry->CompressionBlocks.Last().CompressedEnd - OffsetInPak;
			check(CompressedFileSize > 0);
			const int32 CompressionBlockSize = FileEntry->CompressionBlockSize;
			check((UncompressedFileSize + CompressionBlockSize - 1) / CompressionBlockSize == FileEntry->CompressionBlocks.Num());
			Blocks.AddDefaulted(FileEntry->CompressionBlocks.Num());
		}
		UE_LOG(LogPakFile, Verbose, TEXT("FPakPlatformFile::OpenAsyncRead[%016llX, %016llX) %s"), OffsetInPak, OffsetInPak + CompressedFileSize, Filename);
		check(PakFileSize > 0 && OffsetInPak + CompressedFileSize <= PakFileSize && OffsetInPak >= 0);

		ReadCallbackFunction = [this](bool bWasCancelled, IAsyncReadRequest* Request)
		{
			RawReadCallback(bWasCancelled, Request);
		};

	}
	~FPakAsyncReadFileHandle()
	{
		FScopeLock ScopedLock(&CriticalSection);
		check(!LiveRequests.Num()); // must delete all requests before you delete the handle
		check(!NumLiveRawRequests); // must delete all requests before you delete the handle
		for (FCachedAsyncBlock* Block : Blocks)
		{
			if (Block)
			{
				check(Block->RefCount == 0);
				ClearBlock(*Block, true);
				delete Block;
			}
		}
	}

	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) override
	{
		return new FPakSizeRequest(CompleteCallback, UncompressedFileSize);
	}
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr, uint8* UserSuppliedMemory = nullptr) override
	{
		if (BytesToRead == MAX_int64)
		{
			BytesToRead = UncompressedFileSize - Offset;
		}
		check(Offset + BytesToRead <= UncompressedFileSize && Offset >= 0);
		if (FileEntry->CompressionMethod == COMPRESS_None)
		{
			check(Offset + BytesToRead + OffsetInPak <= PakFileSize);
			check(!Blocks.Num());

			if (FileEntry->bEncrypted)
			{
				return new FPakEncryptedReadRequest(PakFile, PakFileSize, CompleteCallback, OffsetInPak, Offset, BytesToRead, Priority, UserSuppliedMemory);
			}
			else
			{
				return new FPakReadRequest(PakFile, PakFileSize, CompleteCallback, OffsetInPak + Offset, BytesToRead, Priority, UserSuppliedMemory);
			}
		}
		bool bAnyUnfinished = false;
		FPakProcessedReadRequest* Result;
		{
			FScopeLock ScopedLock(&CriticalSection);
			check(Blocks.Num());
			int32 FirstBlock = Offset / FileEntry->CompressionBlockSize;
			int32 LastBlock = (Offset + BytesToRead - 1) / FileEntry->CompressionBlockSize;

			check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

			Result = new FPakProcessedReadRequest(this, CompleteCallback, Offset, BytesToRead, Priority, UserSuppliedMemory);
			for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
			{

				FCachedAsyncBlock& Block = GetBlock(BlockIndex);
				Block.RefCount++;
				if (!Block.bInFlight)
				{
					check(Block.RefCount == 1);
					StartBlock(BlockIndex, Priority);
					bAnyUnfinished = true;
				}
				if (!Block.Processed)
				{
					bAnyUnfinished = true;
				}
			}
			check(!LiveRequests.Contains(Result))
			LiveRequests.Add(Result);
			if (!bAnyUnfinished)
			{
				Result->RequestIsComplete();
			}
		}
		return Result;
	}

	void StartBlock(int32 BlockIndex, EAsyncIOPriority Priority)
	{
		FCachedAsyncBlock& Block = GetBlock(BlockIndex);
		Block.bInFlight = true;
		check(!Block.RawRequest && !Block.Processed && !Block.Raw && !Block.CPUWorkGraphEvent.GetReference() && !Block.ProcessedSize && !Block.RawSize && !Block.bCPUWorkIsComplete);
		Block.RawSize = FileEntry->CompressionBlocks[BlockIndex].CompressedEnd - FileEntry->CompressionBlocks[BlockIndex].CompressedStart;
		if (FileEntry->bEncrypted)
		{
			Block.RawSize = Align(Block.RawSize, FAES::AESBlockSize);
		}
		NumLiveRawRequests++;
		Block.RawRequest = new FPakReadRequest(PakFile, PakFileSize, &ReadCallbackFunction, FileEntry->CompressionBlocks[BlockIndex].CompressedStart, Block.RawSize, Priority, nullptr, true, &Block);
	}
	void RawReadCallback(bool bWasCancelled, IAsyncReadRequest* InRequest)
	{
		// CAUTION, no lock here!
		FPakReadRequest* Request = static_cast<FPakReadRequest*>(InRequest);

		FCachedAsyncBlock& Block = Request->GetBlock();
		check((Block.RawRequest == Request || (!Block.RawRequest && Block.RawSize)) // we still might be in the constructor so the assignment hasn't happened yet
			&& !Block.Processed && !Block.Raw);

		Block.Raw = Request->GetReadResults();
		FPlatformMisc::MemoryBarrier();
		if (Block.bCancelledBlock || !Block.Raw)
		{
			check(Block.bCancelledBlock);
			if (Block.Raw)
			{
				FMemory::Free(Block.Raw);
				Block.Raw = nullptr;
				check(Block.RawSize > 0);
				DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.RawSize);
				Block.RawSize = 0;
			}
		}
		else
		{
			check(Block.Raw);
			Block.ProcessedSize = FileEntry->CompressionBlockSize;
			if (Block.BlockIndex == Blocks.Num() - 1)
			{
				Block.ProcessedSize = FileEntry->UncompressedSize % FileEntry->CompressionBlockSize;
				if (!Block.ProcessedSize)
				{
					Block.ProcessedSize = FileEntry->CompressionBlockSize; // last block was a full block
				}
			}
			check(Block.ProcessedSize && !Block.bCPUWorkIsComplete);
		}
		Block.CPUWorkGraphEvent = TGraphTask<FAsyncIOCPUWorkTask>::CreateTask().ConstructAndDispatchWhenReady(*this, &Block);
	}
	void DoProcessing(FCachedAsyncBlock* BlockPtr)
	{
		FCachedAsyncBlock& Block = *BlockPtr;
		check(!Block.Processed);
		uint8* Output = nullptr;
		if (Block.Raw)
		{
			check(Block.Raw && Block.RawSize && !Block.Processed);

			if (FileEntry->bEncrypted)
			{
				INC_DWORD_STAT(STAT_PakCache_CompressedDecrypts);
				DecryptData(Block.Raw, Align(Block.RawSize, FAES::AESBlockSize));
			}

			check(Block.ProcessedSize > 0);
			INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.ProcessedSize);
			Output = (uint8*)FMemory::Malloc(Block.ProcessedSize);
			FCompression::UncompressMemory((ECompressionFlags)FileEntry->CompressionMethod, Output, Block.ProcessedSize, Block.Raw, Block.RawSize, false, FPlatformMisc::GetPlatformCompression()->GetCompressionBitWindow());
			FMemory::Free(Block.Raw);
			Block.Raw = nullptr;
			check(Block.RawSize > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.RawSize);
			Block.RawSize = 0;
		}
		else
		{
			check(Block.ProcessedSize == 0);
		}

		{
			FScopeLock ScopedLock(&CriticalSection);
			check(!Block.Processed);
			Block.Processed = Output;
			if (Block.RawRequest)
			{
				Block.RawRequest->WaitCompletion();
				delete Block.RawRequest;
				Block.RawRequest = nullptr;
				NumLiveRawRequests--;
			}
			if (Block.RefCount > 0)
			{
				check(&Block == Blocks[Block.BlockIndex] && !Block.bCancelledBlock);
				for (FPakProcessedReadRequest* Req : LiveRequests)
				{
					if (Req->CheckCompletion(*FileEntry, Block.BlockIndex, Blocks))
					{
						Req->RequestIsComplete();
					}
				}
				Block.bCPUWorkIsComplete = true;
			}
			else
			{
				check(&Block != Blocks[Block.BlockIndex] && Block.bCancelledBlock);
				// must have been canceled, clean up
				FPakProcessedReadRequest* Owner;

				check(OutstandingCancelMapBlock.Contains(&Block));
				Owner = OutstandingCancelMapBlock[&Block];
				OutstandingCancelMapBlock.Remove(&Block);
				check(LiveRequests.Contains(Owner));

				if (Owner->CancelBlockComplete(&Block))
				{
					LiveRequests.Remove(Owner);
				}
				ClearBlock(Block);
				delete &Block;
			}
		}
	}
	void ClearBlock(FCachedAsyncBlock& Block, bool bForDestructorShouldAlreadyBeClear = false)
	{
		check(!Block.RawRequest);
		Block.RawRequest = nullptr;
		Block.CPUWorkGraphEvent = nullptr;
		if (Block.Raw)
		{
			check(!bForDestructorShouldAlreadyBeClear);
			// this was a cancel, clean it up now
			FMemory::Free(Block.Raw);
			Block.Raw = nullptr;
			check(Block.RawSize > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.RawSize);
		}
		Block.RawSize = 0;
		if (Block.Processed)
		{
			check(bForDestructorShouldAlreadyBeClear == false);
			FMemory::Free(Block.Processed);
			Block.Processed = nullptr;
			check(Block.ProcessedSize > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.ProcessedSize);
		}
		Block.ProcessedSize = 0;
		Block.bCPUWorkIsComplete = false;
		Block.bInFlight = false;
	}

	void RemoveRequest(FPakProcessedReadRequest* Req, int64 Offset, int64 BytesToRead)
	{
		FScopeLock ScopedLock(&CriticalSection);
		check(LiveRequests.Contains(Req));
		LiveRequests.Remove(Req);
		int32 FirstBlock = Offset / FileEntry->CompressionBlockSize;
		int32 LastBlock = (Offset + BytesToRead - 1) / FileEntry->CompressionBlockSize;
		check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

		for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
		{
			FCachedAsyncBlock& Block = GetBlock(BlockIndex);
			check(Block.RefCount > 0);
			if (!--Block.RefCount)
			{
				if (Block.RawRequest)
				{
					Block.RawRequest->Cancel();
					Block.RawRequest->WaitCompletion();
					delete Block.RawRequest;
					Block.RawRequest = nullptr;
					NumLiveRawRequests--;
				}
				ClearBlock(Block);
			}
		}
	}

	void HandleCanceledRequest(TSet<FCachedAsyncBlock*>& MyCanceledBlocks, FPakProcessedReadRequest* Req, int64 Offset, int64 BytesToRead)
	{
		FScopeLock ScopedLock(&CriticalSection);
		check(LiveRequests.Contains(Req));
		int32 FirstBlock = Offset / FileEntry->CompressionBlockSize;
		int32 LastBlock = (Offset + BytesToRead - 1) / FileEntry->CompressionBlockSize;
		check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

		for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
		{
			FCachedAsyncBlock& Block = GetBlock(BlockIndex);
			check(Block.RefCount > 0);
			if (!--Block.RefCount)
			{
				if (Block.bInFlight && !Block.bCPUWorkIsComplete)
				{
					MyCanceledBlocks.Add(&Block);
					Blocks[BlockIndex] = nullptr;
					check(!OutstandingCancelMapBlock.Contains(&Block));
					OutstandingCancelMapBlock.Add(&Block, Req);
					Block.bCancelledBlock = true;
					FPlatformMisc::MemoryBarrier();
					Block.RawRequest->Cancel();
				}
				else
				{
					ClearBlock(Block);
				}
			}
		}

		if (!MyCanceledBlocks.Num())
		{
			LiveRequests.Remove(Req);
		}
	}


	void GatherResults(uint8* Memory, int64 Offset, int64 BytesToRead)
	{
		// no lock here, I don't think it is needed because we have a ref count.
		int32 FirstBlock = Offset / FileEntry->CompressionBlockSize;
		int32 LastBlock = (Offset + BytesToRead - 1) / FileEntry->CompressionBlockSize;
		check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

		for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
		{
			FCachedAsyncBlock& Block = GetBlock(BlockIndex);
			check(Block.RefCount > 0 && Block.Processed && Block.ProcessedSize);
			int64 BlockStart = int64(BlockIndex) * int64(FileEntry->CompressionBlockSize);
			int64 BlockEnd = BlockStart + Block.ProcessedSize;

			int64 SrcOffset = 0;
			int64 DestOffset = BlockStart - Offset;
			if (DestOffset < 0)
			{
				SrcOffset -= DestOffset;
				DestOffset = 0;
			}
			int64 CopySize = Block.ProcessedSize;
			if (DestOffset + CopySize > BytesToRead)
			{
				CopySize = BytesToRead - DestOffset;
			}
			if (SrcOffset + CopySize > Block.ProcessedSize)
			{
				CopySize = Block.ProcessedSize - SrcOffset;
			}
			check(CopySize > 0 && DestOffset >= 0 && DestOffset + CopySize <= BytesToRead);
			check(SrcOffset >= 0 && SrcOffset + CopySize <= Block.ProcessedSize);
			FMemory::Memcpy(Memory + DestOffset, Block.Processed + SrcOffset, CopySize);

			check(Block.RefCount > 0);
		}
	}
};

void FPakProcessedReadRequest::CancelRawRequests()
{
	bHasCancelled = true;
	Owner->HandleCanceledRequest(MyCanceledBlocks, this, Offset, BytesToRead);
}

void FPakProcessedReadRequest::GatherResults()
{
	if (!bUserSuppliedMemory)
	{
		check(!Memory);
		Memory = (uint8*)FMemory::Malloc(BytesToRead);
		INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
	}
	check(Memory);
	Owner->GatherResults(Memory, Offset, BytesToRead);
}

void FPakProcessedReadRequest::DoneWithRawRequests()
{
	Owner->RemoveRequest(this, Offset, BytesToRead);
}

bool FPakProcessedReadRequest::CheckCompletion(const FPakEntry& FileEntry, int32 BlockIndex, TArray<FCachedAsyncBlock*>& Blocks)
{
	if (!bRequestOutstanding || bHasCompleted || bHasCancelled)
	{
		return false;
	}
	{
		int64 BlockStart = int64(BlockIndex) * int64(FileEntry.CompressionBlockSize);
		int64 BlockEnd = int64(BlockIndex + 1) * int64(FileEntry.CompressionBlockSize);
		if (Offset >= BlockEnd || Offset + BytesToRead <= BlockStart)
		{
			return false;
		}
	}
	int32 FirstBlock = Offset / FileEntry.CompressionBlockSize;
	int32 LastBlock = (Offset + BytesToRead - 1) / FileEntry.CompressionBlockSize;
	check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

	for (int32 MyBlockIndex = FirstBlock; MyBlockIndex <= LastBlock; MyBlockIndex++)
	{
		check(Blocks[MyBlockIndex]);
		if (!Blocks[MyBlockIndex]->Processed)
		{
			return false;
		}
	}
	bHasCompleted = true;
	return true;
}

void FAsyncIOCPUWorkTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	SCOPED_NAMED_EVENT(FAsyncIOCPUWorkTask_DoTask, FColor::Cyan);
	Owner.DoProcessing(BlockPtr);
}

#endif  


IAsyncReadFileHandle* FPakPlatformFile::OpenAsyncRead(const TCHAR* Filename)
{
	check(GConfig);
#if USE_PAK_PRECACHE
	if (FPlatformProcess::SupportsMultithreading() && GPakCache_Enable > 0)
	{
		FPakFile* PakFile = NULL;
		const FPakEntry* FileEntry = FindFileInPakFiles(Filename, &PakFile);
		if (FileEntry && PakFile && PakFile->GetFilenameName() != NAME_None)
		{
			return new FPakAsyncReadFileHandle(FileEntry, PakFile, Filename);
		}
		if (FString(Filename).Contains(TEXT("/Saved/PakFileTest/")))
		{
			UE_LOG(LogPakFile, Error, TEXT("FIle %s has /Saved/PakFileTest/, but was not found."), Filename);
			const FPakEntry* FileEntry2 = FindFileInPakFiles(Filename, &PakFile);


		}
	}
#endif

	return IPlatformFile::OpenAsyncRead(Filename);
}

/**
 * Class to handle correctly reading from a compressed file within a compressed package
 */
class FPakSimpleEncryption
{
public:
	enum
	{
		Alignment = FAES::AESBlockSize,
	};

	static FORCEINLINE int64 AlignReadRequest(int64 Size) 
	{
		return Align(Size, Alignment);
	}

	static FORCEINLINE void DecryptBlock(void* Data, int64 Size)
	{
		INC_DWORD_STAT(STAT_PakCache_SyncDecrypts);
		DecryptData((uint8*)Data, Size);
	}
};

/**
 * Thread local class to manage working buffers for file compression
 */
class FCompressionScratchBuffers : public TThreadSingleton<FCompressionScratchBuffers>
{
public:
	FCompressionScratchBuffers()
		: TempBufferSize(0)
		, ScratchBufferSize(0)
	{}

	int64				TempBufferSize;
	TUniquePtr<uint8[]>	TempBuffer;
	int64				ScratchBufferSize;
	TUniquePtr<uint8[]>	ScratchBuffer;

	void EnsureBufferSpace(int64 CompressionBlockSize, int64 ScrachSize)
	{
		if(TempBufferSize < CompressionBlockSize)
		{
			TempBufferSize = CompressionBlockSize;
			TempBuffer = MakeUnique<uint8[]>(TempBufferSize);
		}
		if(ScratchBufferSize < ScrachSize)
		{
			ScratchBufferSize = ScrachSize;
			ScratchBuffer = MakeUnique<uint8[]>(ScratchBufferSize);
		}
	}
};

/**
 * Class to handle correctly reading from a compressed file within a pak
 */
template< typename EncryptionPolicy = FPakNoEncryption >
class FPakCompressedReaderPolicy
{
public:
	class FPakUncompressTask : public FNonAbandonableTask
	{
	public:
		uint8*				UncompressedBuffer;
		int32				UncompressedSize;
		uint8*				CompressedBuffer;
		int32				CompressedSize;
		ECompressionFlags	Flags;
		void*				CopyOut;
		int64				CopyOffset;
		int64				CopyLength;

		void DoWork()
		{
			// Decrypt and Uncompress from memory to memory.
			int64 EncryptionSize = EncryptionPolicy::AlignReadRequest(CompressedSize);
			EncryptionPolicy::DecryptBlock(CompressedBuffer, EncryptionSize);
			FCompression::UncompressMemory(Flags, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, false, FPlatformMisc::GetPlatformCompression()->GetCompressionBitWindow());
			if (CopyOut)
			{
				FMemory::Memcpy(CopyOut, UncompressedBuffer+CopyOffset, CopyLength);
			}
		}

		FORCEINLINE TStatId GetStatId() const
		{
			// TODO: This is called too early in engine startup.
			return TStatId();
			//RETURN_QUICK_DECLARE_CYCLE_STAT(FPakUncompressTask, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

	FPakCompressedReaderPolicy(const FPakFile& InPakFile, const FPakEntry& InPakEntry, FArchive* InPakReader)
		: PakFile(InPakFile)
		, PakEntry(InPakEntry)
		, PakReader(InPakReader)
	{
	}

	/** Pak file that own this file data */
	const FPakFile&		PakFile;
	/** Pak file entry for this file. */
	const FPakEntry&	PakEntry;
	/** Pak file archive to read the data from. */
	FArchive*			PakReader;

	FORCEINLINE int64 FileSize() const
	{
		return PakEntry.UncompressedSize;
	}

	void Serialize(int64 DesiredPosition, void* V, int64 Length)
	{
		const int32 CompressionBlockSize = PakEntry.CompressionBlockSize;
		uint32 CompressionBlockIndex = DesiredPosition / CompressionBlockSize;
		uint8* WorkingBuffers[2];
		int64 DirectCopyStart = DesiredPosition % PakEntry.CompressionBlockSize;
		FAsyncTask<FPakUncompressTask> UncompressTask;
		FCompressionScratchBuffers& ScratchSpace = FCompressionScratchBuffers::Get();
		bool bStartedUncompress = false;

		int64 WorkingBufferRequiredSize = FCompression::CompressMemoryBound((ECompressionFlags)PakEntry.CompressionMethod,CompressionBlockSize, FPlatformMisc::GetPlatformCompression()->GetCompressionBitWindow());
		WorkingBufferRequiredSize = EncryptionPolicy::AlignReadRequest(WorkingBufferRequiredSize);
		ScratchSpace.EnsureBufferSpace(CompressionBlockSize, WorkingBufferRequiredSize*2);
		WorkingBuffers[0] = ScratchSpace.ScratchBuffer.Get();
		WorkingBuffers[1] = ScratchSpace.ScratchBuffer.Get() + WorkingBufferRequiredSize;

		while (Length > 0)
		{
			const FPakCompressedBlock& Block = PakEntry.CompressionBlocks[CompressionBlockIndex];
			int64 Pos = CompressionBlockIndex * CompressionBlockSize;
			int64 CompressedBlockSize = Block.CompressedEnd-Block.CompressedStart;
			int64 UncompressedBlockSize = FMath::Min<int64>(PakEntry.UncompressedSize-Pos, PakEntry.CompressionBlockSize);
			int64 ReadSize = EncryptionPolicy::AlignReadRequest(CompressedBlockSize);
			int64 WriteSize = FMath::Min<int64>(UncompressedBlockSize - DirectCopyStart, Length);
			PakReader->Seek(Block.CompressedStart);
			PakReader->Serialize(WorkingBuffers[CompressionBlockIndex & 1],ReadSize);
			if (bStartedUncompress)
			{
				UncompressTask.EnsureCompletion();
				bStartedUncompress = false;
			}

			FPakUncompressTask& TaskDetails = UncompressTask.GetTask();
			if (DirectCopyStart == 0 && Length >= CompressionBlockSize)
			{
				// Block can be decompressed directly into output buffer
				TaskDetails.Flags = (ECompressionFlags)PakEntry.CompressionMethod;
				TaskDetails.UncompressedBuffer = (uint8*)V;
				TaskDetails.UncompressedSize = UncompressedBlockSize;
				TaskDetails.CompressedBuffer = WorkingBuffers[CompressionBlockIndex & 1];
				TaskDetails.CompressedSize = CompressedBlockSize;
				TaskDetails.CopyOut = nullptr;
			}
			else
			{
				// Block needs to be copied from a working buffer
				TaskDetails.Flags = (ECompressionFlags)PakEntry.CompressionMethod;
				TaskDetails.UncompressedBuffer = ScratchSpace.TempBuffer.Get();
				TaskDetails.UncompressedSize = UncompressedBlockSize;
				TaskDetails.CompressedBuffer = WorkingBuffers[CompressionBlockIndex & 1];
				TaskDetails.CompressedSize = CompressedBlockSize;
				TaskDetails.CopyOut = V;
				TaskDetails.CopyOffset = DirectCopyStart;
				TaskDetails.CopyLength = WriteSize;
			}
			
			if (Length == WriteSize)
			{
				UncompressTask.StartSynchronousTask();
			}
			else
			{
				UncompressTask.StartBackgroundTask();
			}
			bStartedUncompress = true;
			V = (void*)((uint8*)V + WriteSize);
			Length -= WriteSize;
			DirectCopyStart = 0;
			++CompressionBlockIndex;
		}

		if(bStartedUncompress)
		{
			UncompressTask.EnsureCompletion();
		}
	}
};

bool FPakEntry::VerifyPakEntriesMatch(const FPakEntry& FileEntryA, const FPakEntry& FileEntryB)
{
	bool bResult = true;
	if (FileEntryA.Size != FileEntryB.Size)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header file size mismatch, got: %lld, expected: %lld"), FileEntryB.Size, FileEntryA.Size);
		bResult = false;		
	}
	if (FileEntryA.UncompressedSize != FileEntryB.UncompressedSize)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header uncompressed file size mismatch, got: %lld, expected: %lld"), FileEntryB.UncompressedSize, FileEntryA.UncompressedSize);
		bResult = false;
	}
	if (FileEntryA.CompressionMethod != FileEntryB.CompressionMethod)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header file compression method mismatch, got: %d, expected: %d"), FileEntryB.CompressionMethod, FileEntryA.CompressionMethod);
		bResult = false;
	}
	if (FMemory::Memcmp(FileEntryA.Hash, FileEntryB.Hash, sizeof(FileEntryA.Hash)) != 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak file hash does not match its index entry"));
		bResult = false;
	}
	return bResult;
}

bool FPakPlatformFile::IsNonPakFilenameAllowed(const FString& InFilename)
{
	bool bAllowed = true;

#if EXCLUDE_NONPAK_UE_EXTENSIONS
	if ( PakFiles.Num() || UE_BUILD_SHIPPING)
	{
		FName Ext = FName(*FPaths::GetExtension(InFilename));
		bAllowed = !ExcludedNonPakExtensions.Contains(Ext);
	}
#endif

	FFilenameSecurityDelegate& FilenameSecurityDelegate = GetFilenameSecurityDelegate();
	if (bAllowed)
	{
		if (FilenameSecurityDelegate.IsBound())
		{
			bAllowed = FilenameSecurityDelegate.Execute(*InFilename);;
		}
	}

	return bAllowed;
}


#if IS_PROGRAM
FPakFile::FPakFile(const TCHAR* Filename, bool bIsSigned)
	: PakFilename(Filename)
	, PakFilenameName(Filename)
	, CachedTotalSize(0)
	, bSigned(bIsSigned)
	, bIsValid(false)
{
	FArchive* Reader = GetSharedReader(NULL);
	if (Reader)
	{
		Timestamp = IFileManager::Get().GetTimeStamp(Filename);
		Initialize(Reader);
	}
}
#endif

FPakFile::FPakFile(IPlatformFile* LowerLevel, const TCHAR* Filename, bool bIsSigned)
	: PakFilename(Filename)
	, PakFilenameName(Filename)
	, CachedTotalSize(0)
	, bSigned(bIsSigned)
	, bIsValid(false)
{
	FArchive* Reader = GetSharedReader(LowerLevel);
	if (Reader)
	{
		Timestamp = LowerLevel->GetTimeStamp(Filename);
		Initialize(Reader);
	}
}

#if WITH_EDITOR
FPakFile::FPakFile(FArchive* Archive)
	: bSigned(false)
	, bIsValid(false)
{
	Initialize(Archive);
}
#endif

FPakFile::~FPakFile()
{
}

FArchive* FPakFile::CreatePakReader(const TCHAR* Filename)
{
	FArchive* ReaderArchive = IFileManager::Get().CreateFileReader(Filename);
	return SetupSignedPakReader(ReaderArchive, Filename);
}

FArchive* FPakFile::CreatePakReader(IFileHandle& InHandle, const TCHAR* Filename)
{
	FArchive* ReaderArchive = new FArchiveFileReaderGeneric(&InHandle, Filename, InHandle.Size());
	return SetupSignedPakReader(ReaderArchive, Filename);
}

FArchive* FPakFile::SetupSignedPakReader(FArchive* ReaderArchive, const TCHAR* Filename)
{
	if (FPlatformProperties::RequiresCookedData())
	{
		if (bSigned || FParse::Param(FCommandLine::Get(), TEXT("signedpak")) || FParse::Param(FCommandLine::Get(), TEXT("signed")))
		{
			if (!Decryptor)
			{
				Decryptor = MakeUnique<FChunkCacheWorker>(ReaderArchive, Filename);
			}
			ReaderArchive = new FSignedArchiveReader(ReaderArchive, Decryptor.Get());
		}
	}
	return ReaderArchive;
}

void FPakFile::Initialize(FArchive* Reader)
{
	CachedTotalSize = Reader->TotalSize();

	if (CachedTotalSize < Info.GetSerializedSize())
	{
		if (CachedTotalSize) // UEMOB-425: can be zero - only error when not zero
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Corrupted pak file '%s' (too short). Verify your installation."), *PakFilename);
		}
	}
	else
	{
		// Serialize trailer and check if everything is as expected.
		Reader->Seek(CachedTotalSize - Info.GetSerializedSize());
		Info.Serialize(*Reader);
		UE_CLOG(Info.Magic != FPakInfo::PakFile_Magic, LogPakFile, Fatal, TEXT("Trailing magic number (%ud) in '%s' is different than the expected one. Verify your installation."), Info.Magic, *PakFilename);
		UE_CLOG(!(Info.Version >= FPakInfo::PakFile_Version_Initial && Info.Version <= FPakInfo::PakFile_Version_Latest), LogPakFile, Fatal, TEXT("Invalid pak file version (%d) in '%s'. Verify your installation."), Info.Version, *PakFilename);
		UE_CLOG((Info.bEncryptedIndex == 1) && (FPakPlatformFile::GetPakEncryptionKey() == nullptr), LogPakFile, Fatal, TEXT("Index of pak file '%s' is encrypted, but this executable doesn't have any valid decryption keys"), *PakFilename);

		LoadIndex(Reader);
		// LoadIndex should crash in case of an error, so just assume everything is ok if we got here.
		bIsValid = true;

		if (FParse::Param(FCommandLine::Get(), TEXT("checkpak")))
		{
			ensure(Check());
		}
	}
}

void FPakFile::LoadIndex(FArchive* Reader)
{
	if (CachedTotalSize < (Info.IndexOffset + Info.IndexSize))
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Corrupted index offset in pak file."));
	}
	else
	{
		// Load index into memory first.
		Reader->Seek(Info.IndexOffset);
		TArray<uint8> IndexData;
		IndexData.AddUninitialized(Info.IndexSize);
		Reader->Serialize(IndexData.GetData(), Info.IndexSize);
		FMemoryReader IndexReader(IndexData);

		// Decrypt if necessary
		if (Info.bEncryptedIndex)
		{
			DecryptData(IndexData.GetData(), Info.IndexSize);
		}

		// Check SHA1 value.
		uint8 IndexHash[20];
		FSHA1::HashBuffer(IndexData.GetData(), IndexData.Num(), IndexHash);
		if (FMemory::Memcmp(IndexHash, Info.IndexHash, sizeof(IndexHash)) != 0)
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Corrupted index in pak file (CRC mismatch)."));
		}

		// Read the default mount point and all entries.
		int32 NumEntries = 0;
		IndexReader << MountPoint;
		IndexReader << NumEntries;

		MakeDirectoryFromPath(MountPoint);
		// Allocate enough memory to hold all entries (and not reallocate while they're being added to it).
		Files.Empty(NumEntries);

		for (int32 EntryIndex = 0; EntryIndex < NumEntries; EntryIndex++)
		{
			// Serialize from memory.
			FPakEntry Entry;
			FString Filename;
			IndexReader << Filename;
			Entry.Serialize(IndexReader, Info.Version);

			// Add new file info.
			Files.Add(Entry);

			// Construct Index of all directories in pak file.
			FString Path = FPaths::GetPath(Filename);
			MakeDirectoryFromPath(Path);
			FPakDirectory* Directory = Index.Find(Path);
			if (Directory != NULL)
			{
				Directory->Add(FPaths::GetCleanFilename(Filename), &Files.Last());	
			}
			else
			{
				FPakDirectory& NewDirectory = Index.Add(Path);
				NewDirectory.Add(FPaths::GetCleanFilename(Filename), &Files.Last());

				// add the parent directories up to the mount point
				while (MountPoint != Path)
				{
					Path = Path.Left(Path.Len()-1);
					int32 Offset = 0;
					if (Path.FindLastChar('/', Offset))
					{
						Path = Path.Left(Offset);
						MakeDirectoryFromPath(Path);
						if (Index.Find(Path) == NULL)
						{
							Index.Add(Path);
						}
					}
					else
					{
						Path = MountPoint;
					}
				}
			}
		}
	}
}

bool FPakFile::Check()
{
	UE_LOG(LogPakFile, Display, TEXT("Checking pak file \"%s\". This may take a while..."), *PakFilename);
	FArchive& PakReader = *GetSharedReader(NULL);
	int32 ErrorCount = 0;
	int32 FileCount = 0;

	for (FPakFile::FFileIterator It(*this); It; ++It, ++FileCount)
	{
		const FPakEntry& Entry = It.Info();
		void* FileContents = FMemory::Malloc(Entry.Size);
		PakReader.Seek(Entry.Offset);
		uint32 SerializedCrcTest = 0;
		FPakEntry EntryInfo;
		EntryInfo.Serialize(PakReader, GetInfo().Version);
		if (EntryInfo != Entry)
		{
			UE_LOG(LogPakFile, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
			ErrorCount++;
		}
		PakReader.Serialize(FileContents, Entry.Size);

		uint8 TestHash[20];
		FSHA1::HashBuffer(FileContents, Entry.Size, TestHash);
		if (FMemory::Memcmp(TestHash, Entry.Hash, sizeof(TestHash)) != 0)
		{
			UE_LOG(LogPakFile, Error, TEXT("Hash mismatch for \"%s\"."), *It.Filename());
			ErrorCount++;
		}
		else
		{
			UE_LOG(LogPakFile, Display, TEXT("\"%s\" OK."), *It.Filename());
		}
		FMemory::Free(FileContents);
	}
	if (ErrorCount == 0)
	{
		UE_LOG(LogPakFile, Display, TEXT("Pak file \"%s\" healthy, %d files checked."), *PakFilename, FileCount);
	}
	else
	{
		UE_LOG(LogPakFile, Display, TEXT("Pak file \"%s\" corrupted (%d errors ouf of %d files checked.)."), *PakFilename, ErrorCount, FileCount);
	}

	return ErrorCount == 0;
}

#if DO_CHECK
/**
* FThreadCheckingArchiveProxy - checks that inner archive is only used from the specified thread ID
*/
class FThreadCheckingArchiveProxy : public FArchiveProxy
{
public:

	const uint32 ThreadId;
	FArchive* InnerArchivePtr;

	FThreadCheckingArchiveProxy(FArchive* InReader, uint32 InThreadId)
		: FArchiveProxy(*InReader)
		, ThreadId(InThreadId)
		, InnerArchivePtr(InReader)
	{}

	virtual ~FThreadCheckingArchiveProxy() 
	{
		if (InnerArchivePtr)
		{
			delete InnerArchivePtr;
		}
	}

	//~ Begin FArchiveProxy Interface
	virtual void Serialize(void* Data, int64 Length) override
	{
		if (FPlatformTLS::GetCurrentThreadId() != ThreadId)
		{
			UE_LOG(LogPakFile, Error, TEXT("Attempted serialize using thread-specific pak file reader on the wrong thread.  Reader for thread %d used by thread %d."), ThreadId, FPlatformTLS::GetCurrentThreadId());
		}
		InnerArchive.Serialize(Data, Length);
	}

	virtual void Seek(int64 InPos) override
	{
		if (FPlatformTLS::GetCurrentThreadId() != ThreadId)
		{
			UE_LOG(LogPakFile, Error, TEXT("Attempted seek using thread-specific pak file reader on the wrong thread.  Reader for thread %d used by thread %d."), ThreadId, FPlatformTLS::GetCurrentThreadId());
		}
		InnerArchive.Seek(InPos);
	}
	//~ End FArchiveProxy Interface
};
#endif //DO_CHECK

FArchive* FPakFile::GetSharedReader(IPlatformFile* LowerLevel)
{
	uint32 Thread = FPlatformTLS::GetCurrentThreadId();
	FArchive* PakReader = NULL;
	{
		FScopeLock ScopedLock(&CriticalSection);
		TUniquePtr<FArchive>* ExistingReader = ReaderMap.Find(Thread);
		if (ExistingReader)
		{
			PakReader = ExistingReader->Get();
		}
	}
	if (!PakReader)
	{
		// Create a new FArchive reader and pass it to the new handle.
		if (LowerLevel != NULL)
		{
			IFileHandle* PakHandle = LowerLevel->OpenRead(*GetFilename());
			if (PakHandle)
			{
				PakReader = CreatePakReader(*PakHandle, *GetFilename());
			}
		}
		else
		{
			PakReader = CreatePakReader(*GetFilename());
		}
		if (!PakReader)
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Unable to create pak \"%s\" handle"), *GetFilename());
		}
		{
			FScopeLock ScopedLock(&CriticalSection);
#if DO_CHECK
			ReaderMap.Emplace(Thread, new FThreadCheckingArchiveProxy(PakReader, Thread));
#else //DO_CHECK
			ReaderMap.Emplace(Thread, PakReader);
#endif //DO_CHECK
		}
	}
	return PakReader;
}

#if !UE_BUILD_SHIPPING
class FPakExec : private FSelfRegisteringExec
{
	FPakPlatformFile& PlatformFile;

public:

	FPakExec(FPakPlatformFile& InPlatformFile)
		: PlatformFile(InPlatformFile)
	{}

	/** Console commands **/
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if (FParse::Command(&Cmd, TEXT("Mount")))
		{
			PlatformFile.HandleMountCommand(Cmd, Ar);
			return true;
		}
		if (FParse::Command(&Cmd, TEXT("Unmount")))
		{
			PlatformFile.HandleUnmountCommand(Cmd, Ar);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PakList")))
		{
			PlatformFile.HandlePakListCommand(Cmd, Ar);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PakCorrupt")))
		{
			PlatformFile.HandlePakCorruptCommand(Cmd, Ar);
			return true;
		}
		return false;
	}
};
static TUniquePtr<FPakExec> GPakExec;

void FPakPlatformFile::HandleMountCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString PakFilename = FParse::Token(Cmd, false);
	if (!PakFilename.IsEmpty())
	{
		const FString MountPoint = FParse::Token(Cmd, false);
		Mount(*PakFilename, 0, MountPoint.IsEmpty() ? NULL : *MountPoint);
	}
}

void FPakPlatformFile::HandleUnmountCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString PakFilename = FParse::Token(Cmd, false);
	if (!PakFilename.IsEmpty())
	{
		Unmount(*PakFilename);
	}
}

void FPakPlatformFile::HandlePakListCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);
	for (auto Pak : Paks)
	{
		Ar.Logf(TEXT("%s Mounted to %s"), *Pak.PakFile->GetFilename(), *Pak.PakFile->GetMountPoint());
	}	
}

void FPakPlatformFile::HandlePakCorruptCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
#if USE_PAK_PRECACHE
	FPakPrecacher::Get().SimulatePakFileCorruption();
#endif
}
#endif // !UE_BUILD_SHIPPING

FPakPlatformFile::FPakPlatformFile()
	: LowerLevel(NULL)
	, bSigned(false)
{
}

FPakPlatformFile::~FPakPlatformFile()
{
	FCoreDelegates::OnMountPak.Unbind();
	FCoreDelegates::OnUnmountPak.Unbind();

#if USE_PAK_PRECACHE
	FPakPrecacher::Shutdown();
#endif
	{
		FScopeLock ScopedLock(&PakListCritical);
		for (int32 PakFileIndex = 0; PakFileIndex < PakFiles.Num(); PakFileIndex++)
		{
			delete PakFiles[PakFileIndex].PakFile;
			PakFiles[PakFileIndex].PakFile = nullptr;
		}
	}	
}

void FPakPlatformFile::FindPakFilesInDirectory(IPlatformFile* LowLevelFile, const TCHAR* Directory, TArray<FString>& OutPakFiles)
{
	// Helper class to find all pak files.
	class FPakSearchVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FoundPakFiles;
		IPlatformChunkInstall* ChunkInstall;
	public:
		FPakSearchVisitor(TArray<FString>& InFoundPakFiles, IPlatformChunkInstall* InChunkInstall)
			: FoundPakFiles(InFoundPakFiles)
			, ChunkInstall(InChunkInstall)
		{}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory == false)
			{
				FString Filename(FilenameOrDirectory);
				if (FPaths::GetExtension(Filename) == TEXT("pak"))
				{
					// if a platform supports chunk style installs, make sure that the chunk a pak file resides in is actually fully installed before accepting pak files from it
					if (ChunkInstall)
					{
						FString ChunkIdentifier(TEXT("pakchunk"));
						FString BaseFilename = FPaths::GetBaseFilename(Filename);
						if (BaseFilename.StartsWith(ChunkIdentifier))
						{
							int32 DelimiterIndex = 0;
							int32 StartOfChunkIndex = ChunkIdentifier.Len();

							BaseFilename.FindChar(TEXT('-'), DelimiterIndex);
							FString ChunkNumberString = BaseFilename.Mid(StartOfChunkIndex, DelimiterIndex-StartOfChunkIndex);
							int32 ChunkNumber = 0;
							TTypeFromString<int32>::FromString(ChunkNumber, *ChunkNumberString);
							if (ChunkInstall->GetChunkLocation(ChunkNumber) == EChunkLocation::NotAvailable)
							{
								return true;
							}
						}
					}
					FoundPakFiles.Add(Filename);
				}
			}
			return true;
		}
	};
	// Find all pak files.
	FPakSearchVisitor Visitor(OutPakFiles, FPlatformMisc::GetPlatformChunkInstall());
	LowLevelFile->IterateDirectoryRecursively(Directory, Visitor);
}

void FPakPlatformFile::FindAllPakFiles(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders, TArray<FString>& OutPakFiles)
{
	// Find pak files from the specified directories.	
	for (int32 FolderIndex = 0; FolderIndex < PakFolders.Num(); ++FolderIndex)
	{
		FindPakFilesInDirectory(LowLevelFile, *PakFolders[FolderIndex], OutPakFiles);
	}
}

void FPakPlatformFile::GetPakFolders(const TCHAR* CmdLine, TArray<FString>& OutPakFolders)
{
#if !UE_BUILD_SHIPPING
	// Command line folders
	FString PakDirs;
	if (FParse::Value(CmdLine, TEXT("-pakdir="), PakDirs))
	{
		TArray<FString> CmdLineFolders;
		PakDirs.ParseIntoArray(CmdLineFolders, TEXT("*"), true);
		OutPakFolders.Append(CmdLineFolders);
	}
#endif

	// @todo plugin urgent: Needs to handle plugin Pak directories, too
	// Hardcoded locations
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::ProjectContentDir()));
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::ProjectSavedDir()));
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::EngineContentDir()));
}

bool FPakPlatformFile::CheckIfPakFilesExist(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders)
{
	TArray<FString> FoundPakFiles;
	FindAllPakFiles(LowLevelFile, PakFolders, FoundPakFiles);
	return FoundPakFiles.Num() > 0;
}

bool FPakPlatformFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
	bool Result = false;
	if (FPlatformProperties::RequiresCookedData() && !FParse::Param(CmdLine, TEXT("NoPak")))
	{
		TArray<FString> PakFolders;
		GetPakFolders(CmdLine, PakFolders);
		Result = CheckIfPakFilesExist(Inner, PakFolders);
	}
	return Result;
}

bool FPakPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	// Inner is required.
	check(Inner != NULL);
	LowerLevel = Inner;

#if EXCLUDE_NONPAK_UE_EXTENSIONS
	// Extensions for file types that should only ever be in a pak file. Used to stop unnecessary access to the lower level platform file
	ExcludedNonPakExtensions.Add(TEXT("uasset"));
	ExcludedNonPakExtensions.Add(TEXT("umap"));
	ExcludedNonPakExtensions.Add(TEXT("ubulk"));
	ExcludedNonPakExtensions.Add(TEXT("uexp"));
#endif

	FEncryptionKey DecryptionKey;
	FString PakSigningKeyExponent, PakSigningKeyModulus;
	GetPakSigningKeys(PakSigningKeyExponent, PakSigningKeyModulus);
	DecryptionKey.Exponent.Parse(PakSigningKeyExponent);
	DecryptionKey.Modulus.Parse(PakSigningKeyModulus);

	bSigned = !DecryptionKey.Exponent.IsZero() && !DecryptionKey.Modulus.IsZero();
	
	bool bMountPaks = true;
	TArray<FString> PaksToLoad;
#if !UE_BUILD_SHIPPING
	// Optionally get a list of pak filenames to load, only these paks will be mounted
	FString CmdLinePaksToLoad;
	if (FParse::Value(CmdLine, TEXT("-paklist="), CmdLinePaksToLoad))
	{
		CmdLinePaksToLoad.ParseIntoArray(PaksToLoad, TEXT("+"), true);
	}

	//if we are using a fileserver, then dont' mount paks automatically.  We only want to read files from the server.
	FString FileHostIP;
	const bool bCookOnTheFly = FParse::Value(FCommandLine::Get(), TEXT("filehostip"), FileHostIP);
	const bool bPreCookedNetwork = FParse::Param(FCommandLine::Get(), TEXT("precookednetwork") );
	if (bPreCookedNetwork)
	{
		// precooked network builds are dependent on cook on the fly
		check(bCookOnTheFly);
	}
	bMountPaks &= (!bCookOnTheFly || bPreCookedNetwork);
#endif

	if (bMountPaks)
	{	
		// Find and mount pak files from the specified directories.
		TArray<FString> PakFolders;
		GetPakFolders(CmdLine, PakFolders);
		TArray<FString> FoundPakFiles;
		FindAllPakFiles(LowerLevel, PakFolders, FoundPakFiles);
		// Sort in descending order.
		FoundPakFiles.Sort(TGreater<FString>());
		// Mount all found pak files
		for (int32 PakFileIndex = 0; PakFileIndex < FoundPakFiles.Num(); PakFileIndex++)
		{
			const FString& PakFilename = FoundPakFiles[PakFileIndex];
			bool bLoadPak = true;
			if (PaksToLoad.Num() && !PaksToLoad.Contains(FPaths::GetBaseFilename(PakFilename)))
			{
				bLoadPak = false;
			}
			if (bLoadPak)
			{
				// hardcode default load ordering of game main pak -> game content -> engine content -> saved dir
				// would be better to make this config but not even the config system is initialized here so we can't do that
				uint32 PakOrder = 0;
				if (PakFilename.StartsWith(FString::Printf(TEXT("%sPaks/%s-"), *FPaths::ProjectContentDir(), FApp::GetProjectName())))
				{
					PakOrder = 4;
				}
				else if (PakFilename.StartsWith(FPaths::ProjectContentDir()))
				{
					PakOrder = 3;
				}
				else if (PakFilename.StartsWith(FPaths::EngineContentDir()))
				{
					PakOrder = 2;
				}
				else if (PakFilename.StartsWith(FPaths::ProjectSavedDir()))
				{
					PakOrder = 1;
				}

				Mount(*PakFilename, PakOrder);
			}
		}
	}

#if !UE_BUILD_SHIPPING
	GPakExec = MakeUnique<FPakExec>(*this);
#endif // !UE_BUILD_SHIPPING

	FCoreDelegates::OnMountPak.BindRaw(this, &FPakPlatformFile::HandleMountPakDelegate);
	FCoreDelegates::OnUnmountPak.BindRaw(this, &FPakPlatformFile::HandleUnmountPakDelegate);


	return !!LowerLevel;
}

void FPakPlatformFile::InitializeNewAsyncIO()
{
#if USE_PAK_PRECACHE 
	if (!WITH_EDITOR && FPlatformProcess::SupportsMultithreading() &&  !FParse::Param(FCommandLine::Get(), TEXT("FileOpenLog")))
	{
		FEncryptionKey DecryptionKey;
		FString PakSigningKeyExponent, PakSigningKeyModulus;
		GetPakSigningKeys(PakSigningKeyExponent, PakSigningKeyModulus);
		DecryptionKey.Exponent.Parse(PakSigningKeyExponent);
		DecryptionKey.Modulus.Parse(PakSigningKeyModulus);

		FPakPrecacher::Init(LowerLevel, DecryptionKey);
	}
	else
	{
		UE_CLOG(FParse::Param(FCommandLine::Get(), TEXT("FileOpenLog")), LogPakFile, Display, TEXT("Disabled pak precacher to get an accurate load order. This should only be used to collect gameopenorder.log, as it is quite slow."));
		GPakCache_Enable = 0;
	}
#endif
}

bool FPakPlatformFile::Mount(const TCHAR* InPakFilename, uint32 PakOrder, const TCHAR* InPath /*= NULL*/)
{
	bool bSuccess = false;
	TSharedPtr<IFileHandle> PakHandle = MakeShareable(LowerLevel->OpenRead(InPakFilename));
	if (PakHandle.IsValid())
	{
		FPakFile* Pak = new FPakFile(LowerLevel, InPakFilename, bSigned);
		if (Pak->IsValid())
		{
			if (InPath != NULL)
			{
				Pak->SetMountPoint(InPath);
			}
			FString PakFilename = InPakFilename;
			if ( PakFilename.EndsWith(TEXT("_P.pak")) )
			{
				// Prioritize based on the chunk version number
				// Default to version 1 for single patch system
				uint32 ChunkVersionNumber = 1;
				FString StrippedPakFilename = PakFilename.LeftChop(6);
				int32 VersionStartIndex = PakFilename.Find("_", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				if (VersionStartIndex != INDEX_NONE)
				{
					FString VersionString = PakFilename.RightChop(VersionStartIndex);
					if (VersionString.IsNumeric())
					{
						int32 ChunkVersionSigned = FCString::Atoi(*VersionString);
						if (ChunkVersionSigned >= 1)
						{
							// Increment by one so that the first patch file still gets more priority than the base pak file
							ChunkVersionNumber = (uint32)ChunkVersionSigned + 1;
						}
					}
				}
				PakOrder += 100 * ChunkVersionNumber;
			}
			{
				// Add new pak file
				FScopeLock ScopedLock(&PakListCritical);
				FPakListEntry Entry;
				Entry.ReadOrder = PakOrder;
				Entry.PakFile = Pak;
				PakFiles.Add(Entry);
				PakFiles.StableSort();
			}
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Failed to mount pak \"%s\", pak is invalid."), InPakFilename);
		}
	}
	else
	{
		UE_LOG(LogPakFile, Warning, TEXT("Pak \"%s\" does not exist!"), InPakFilename);
	}
	return bSuccess;
}

bool FPakPlatformFile::Unmount(const TCHAR* InPakFilename)
{
#if USE_PAK_PRECACHE
	if (GPakCache_Enable)
	{
		FPakPrecacher::Get().Unmount(InPakFilename);
	}
#endif
	{
		FScopeLock ScopedLock(&PakListCritical); 

		for (int32 PakIndex = 0; PakIndex < PakFiles.Num(); PakIndex++)
		{
			if (PakFiles[PakIndex].PakFile->GetFilename() == InPakFilename)
			{
				delete PakFiles[PakIndex].PakFile;
				PakFiles.RemoveAt(PakIndex);
				return true;
			}
		}
	}
	return false;
}

IFileHandle* FPakPlatformFile::CreatePakFileHandle(const TCHAR* Filename, FPakFile* PakFile, const FPakEntry* FileEntry)
{
	IFileHandle* Result = NULL;
	bool bNeedsDelete = true;
	FArchive* PakReader = PakFile->GetSharedReader(LowerLevel);

	// Create the handle.
	if (FileEntry->CompressionMethod != COMPRESS_None && PakFile->GetInfo().Version >= FPakInfo::PakFile_Version_CompressionEncryption)
	{
		if (FileEntry->bEncrypted)
		{
			Result = new FPakFileHandle< FPakCompressedReaderPolicy<FPakSimpleEncryption> >(*PakFile, *FileEntry, PakReader, bNeedsDelete);
		}
		else
		{
			Result = new FPakFileHandle< FPakCompressedReaderPolicy<> >(*PakFile, *FileEntry, PakReader, bNeedsDelete);
		}
	}
	else if (FileEntry->bEncrypted)
	{
		Result = new FPakFileHandle< FPakReaderPolicy<FPakSimpleEncryption> >(*PakFile, *FileEntry, PakReader, bNeedsDelete);
	}
	else
	{
		Result = new FPakFileHandle<>(*PakFile, *FileEntry, PakReader, bNeedsDelete);
	}

	return Result;
}

bool FPakPlatformFile::HandleMountPakDelegate(const FString& PakFilePath, uint32 PakOrder, IPlatformFile::FDirectoryVisitor* Visitor)
{
	bool bReturn = Mount(*PakFilePath, PakOrder);
	if (bReturn && Visitor != nullptr)
	{
		TArray<FPakListEntry> Paks;
		GetMountedPaks(Paks);
		// Find the single pak we just mounted
		for (auto Pak : Paks)
		{
			if (PakFilePath == Pak.PakFile->GetFilename())
			{
				// Get a list of all of the files in the pak
				for (FPakFile::FFileIterator It(*Pak.PakFile); It; ++It)
				{
					Visitor->Visit(*It.Filename(), false);
				}
				return true;
			}
		}
	}
	return bReturn;
}

bool FPakPlatformFile::HandleUnmountPakDelegate(const FString& PakFilePath)
{
	return Unmount(*PakFilePath);
}

IFileHandle* FPakPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	IFileHandle* Result = NULL;
	FPakFile* PakFile = NULL;
	const FPakEntry* FileEntry = FindFileInPakFiles(Filename, &PakFile);	
	if (FileEntry != NULL)
	{
		Result = CreatePakFileHandle(Filename, PakFile, FileEntry);
	}
	else
	{
		if (IsNonPakFilenameAllowed(Filename))
		{
			// Default to wrapped file
			Result = LowerLevel->OpenRead(Filename, bAllowWrite);
		}
	}
	return Result;
}

bool FPakPlatformFile::BufferedCopyFile(IFileHandle& Dest, IFileHandle& Source, const int64 FileSize, uint8* Buffer, const int64 BufferSize) const
{	
	int64 RemainingSizeToCopy = FileSize;
	// Continue copying chunks using the buffer
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		if (Source.Read(Buffer, SizeToCopy) == false)
		{
			return false;
		}
		if (Dest.Write(Buffer, SizeToCopy) == false)
		{
			return false;
		}
		RemainingSizeToCopy -= SizeToCopy;
	}
	return true;
}

bool FPakPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags, EPlatformFileWrite WriteFlags)
{
	bool Result = false;
	FPakFile* PakFile = NULL;
	const FPakEntry* FileEntry = FindFileInPakFiles(From, &PakFile);	
	if (FileEntry != NULL)
	{
		// Copy from pak to LowerLevel->
		// Create handles both files.
		TUniquePtr<IFileHandle> DestHandle(LowerLevel->OpenWrite(To, false, (WriteFlags & EPlatformFileWrite::AllowRead) != EPlatformFileWrite::None));
		TUniquePtr<IFileHandle> SourceHandle(CreatePakFileHandle(From, PakFile, FileEntry));

		if (DestHandle && SourceHandle)
		{
			const int64 BufferSize = 64 * 1024; // Copy in 64K chunks.
			uint8* Buffer = (uint8*)FMemory::Malloc(BufferSize);
			Result = BufferedCopyFile(*DestHandle, *SourceHandle, SourceHandle->Size(), Buffer, BufferSize);
			FMemory::Free(Buffer);
		}
	}
	else
	{
		Result = LowerLevel->CopyFile(To, From, ReadFlags, WriteFlags);
	}
	return Result;
}

/**
 * Module for the pak file
 */
class FPakFileModule : public IPlatformFileModule
{
public:
	virtual IPlatformFile* GetPlatformFile() override
	{
		static TUniquePtr<IPlatformFile> AutoDestroySingleton = MakeUnique<FPakPlatformFile>();
		return AutoDestroySingleton.Get();
	}
};

IMPLEMENT_MODULE(FPakFileModule, PakFile);
