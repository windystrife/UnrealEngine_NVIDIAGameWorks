// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformMemory.h: Apple platform memory functions common across all Apple OSes
=============================================================================*/

#include "ApplePlatformMemory.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Templates/AlignmentTemplates.h"

#include <stdlib.h>
#include "Misc/AssertionMacros.h"
#include "Misc/CoreStats.h"
#include "MallocAnsi.h"
#include "MallocBinned.h"
#include "MallocBinned2.h"
#include "MallocStomp.h"
#include "CoreGlobals.h"

#include <stdlib.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <objc/runtime.h>
#include <CoreFoundation/CFBase.h>

NS_ASSUME_NONNULL_BEGIN

/** 
 * Zombie object implementation so that we can implement NSZombie behaviour for our custom allocated objects.
 * Will leak memory - just like Cocoa's NSZombie - but allows for debugging of invalid usage of the pooled types.
 */
@interface FApplePlatformObjectZombie : NSObject 
{
	@public
	Class OriginalClass;
}
@end

@implementation FApplePlatformObjectZombie
-(id)init
{
	self = (FApplePlatformObjectZombie*)[super init];
	if (self)
	{
		OriginalClass = nil;
	}
	return self;
}

-(void)dealloc
{
	// Denied!
	return;
	
	[super dealloc];
}

- (nullable NSMethodSignature *)methodSignatureForSelector:(SEL)sel
{
	NSLog(@"Selector %@ sent to deallocated instance %p of class %@", NSStringFromSelector(sel), self, OriginalClass);
	abort();
}
@end

@implementation FApplePlatformObject

+ (nullable OSQueueHead*)classAllocator
{
	return nullptr;
}

+ (id)allocClass: (Class)NewClass
{
	static bool NSZombieEnabled = (getenv("NSZombieEnabled") != nullptr);
	
	// Allocate the correct size & zero it
	// All allocations must be 16 byte aligned
	SIZE_T Size = Align(FPlatformMath::Max(class_getInstanceSize(NewClass), class_getInstanceSize([FApplePlatformObjectZombie class])), 16);
	void* Mem = nullptr;
	
	OSQueueHead* Alloc = [NewClass classAllocator];
	if (Alloc && !NSZombieEnabled)
	{
		Mem = OSAtomicDequeue(Alloc, 0);
		if (!Mem)
		{
			static uint8 BlocksPerChunk = 32;
			char* Chunk = (char*)FMemory::Malloc(Size * BlocksPerChunk);
			Mem = Chunk;
			Chunk += Size;
			for (uint8 i = 0; i < (BlocksPerChunk - 1); i++, Chunk += Size)
			{
				OSAtomicEnqueue(Alloc, Chunk, 0);
			}
		}
	}
	else
	{
		Mem = FMemory::Malloc(Size);
	}
	FMemory::Memzero(Mem, Size);
	
	// Construction assumes & requires zero-initialised memory
	FApplePlatformObject* Obj = (FApplePlatformObject*)objc_constructInstance(NewClass, Mem);
	object_setClass(Obj, NewClass);
	Obj->AllocatorPtr = !NSZombieEnabled ? Alloc : nullptr;
	return Obj;
}

- (void)dealloc
{
	static bool NSZombieEnabled = (getenv("NSZombieEnabled") != nullptr);
	
	// First call the destructor and then release the memory - like C++ placement new/delete
	objc_destructInstance(self);
	if (AllocatorPtr)
	{
		check(!NSZombieEnabled);
		OSAtomicEnqueue(AllocatorPtr, self, 0);
	}
	else if (NSZombieEnabled)
	{
		Class CurrentClass = self.class;
		object_setClass(self, [FApplePlatformObjectZombie class]);
		FApplePlatformObjectZombie* ZombieSelf = (FApplePlatformObjectZombie*)self;
		ZombieSelf->OriginalClass = CurrentClass;
	}
	else
	{
		FMemory::Free(self);
	}
	return;
	
	// Deliberately unreachable code to silence clang's error about not calling super - which in all other
	// cases will be correct.
	[super dealloc];
}

@end

static void* FApplePlatformAllocatorAllocate(CFIndex AllocSize, CFOptionFlags Hint, void* Info)
{
	void* Mem = FMemory::Malloc(AllocSize, 16);
	return Mem;
}

static void* FApplePlatformAllocatorReallocate(void* Ptr, CFIndex Newsize, CFOptionFlags Hint, void* Info)
{
	void* Mem = FMemory::Realloc(Ptr, Newsize, 16);
	return Mem;
}

static void FApplePlatformAllocatorDeallocate(void* Ptr, void* Info)
{
	return FMemory::Free(Ptr);
}

static CFIndex FApplePlatformAllocatorPreferredSize(CFIndex Size, CFOptionFlags Hint, void* Info)
{
	return FMemory::QuantizeSize(Size);
}

void FApplePlatformMemory::ConfigureDefaultCFAllocator(void)
{
	// Configure CoreFoundation's default allocator to use our allocation routines too.
	CFAllocatorContext AllocatorContext;
	AllocatorContext.version = 0;
	AllocatorContext.info = nullptr;
	AllocatorContext.retain = nullptr;
	AllocatorContext.release = nullptr;
	AllocatorContext.copyDescription = nullptr;
	AllocatorContext.allocate = &FApplePlatformAllocatorAllocate;
	AllocatorContext.reallocate = &FApplePlatformAllocatorReallocate;
	AllocatorContext.deallocate = &FApplePlatformAllocatorDeallocate;
	AllocatorContext.preferredSize = &FApplePlatformAllocatorPreferredSize;
	
	CFAllocatorRef Alloc = CFAllocatorCreate(kCFAllocatorDefault, &AllocatorContext);
	CFAllocatorSetDefault(Alloc);
}

void FApplePlatformMemory::Init()
{
	FGenericPlatformMemory::Init();
	
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) Pagefile=%.1fGB Virtual=%.1fGB"),
		   float(MemoryConstants.TotalPhysical/1024.0/1024.0/1024.0),
		   MemoryConstants.TotalPhysicalGB,
		   float((MemoryConstants.TotalVirtual-MemoryConstants.TotalPhysical)/1024.0/1024.0/1024.0),
		   float(MemoryConstants.TotalVirtual/1024.0/1024.0/1024.0) );
}

// Set rather to use BinnedMalloc2 for binned malloc, can be overridden below
#define USE_MALLOC_BINNED2 (PLATFORM_MAC)

FMalloc* FApplePlatformMemory::BaseAllocator()
{
	if (FORCE_ANSI_ALLOCATOR)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}
	else if (USE_MALLOC_STOMP)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Stomp;
	}
	else if (USE_MALLOC_BINNED2)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned2;
	}
	else
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned;
	}
	
	// Force ansi malloc in some cases
	if(getenv("UE4_FORCE_MALLOC_ANSI") != nullptr)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}
	
	switch (AllocatorToUse)
	{
		case EMemoryAllocatorToUse::Ansi:
			return new FMallocAnsi();
#if USE_MALLOC_STOMP
		case EMemoryAllocatorToUse::Stomp:
			return new FMallocStomp();
#endif
		case EMemoryAllocatorToUse::Binned2:
			return new FMallocBinned2();
			
		default:	// intentional fall-through
		case EMemoryAllocatorToUse::Binned:
		{
			// get free memory
			vm_statistics Stats;
			mach_msg_type_number_t StatsSize = sizeof(Stats);
			host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
			// 1 << FMath::CeilLogTwo(MemoryConstants.TotalPhysical) should really be FMath::RoundUpToPowerOfTwo,
			// but that overflows to 0 when MemoryConstants.TotalPhysical is close to 4GB, since CeilLogTwo returns 32
			// this then causes the MemoryLimit to be 0 and crashing the app
			uint64 MemoryLimit = FMath::Min<uint64>( uint64(1) << FMath::CeilLogTwo(Stats.free_count * GetConstants().PageSize), 0x100000000);
			
			// [RCL] 2017-03-06 FIXME: perhaps BinnedPageSize should be used here, but leaving this change to the Mac platform owner.
			return new FMallocBinned((uint32)(GetConstants().PageSize&MAX_uint32), MemoryLimit);
		}
	}
	
}

FPlatformMemoryStats FApplePlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	
	FPlatformMemoryStats MemoryStats;
	
	// Gather platform memory stats.
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	uint64_t FreeMem = Stats.free_count * MemoryConstants.PageSize;
	MemoryStats.AvailablePhysical = FreeMem;
	
	// Just get memory information for the process and report the working set instead
	mach_task_basic_info_data_t TaskInfo;
	mach_msg_type_number_t TaskInfoCount = MACH_TASK_BASIC_INFO_COUNT;
	task_info( mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&TaskInfo, &TaskInfoCount );
	MemoryStats.UsedPhysical = TaskInfo.resident_size;
	if(MemoryStats.UsedPhysical > MemoryStats.PeakUsedPhysical)
	{
		MemoryStats.PeakUsedPhysical = MemoryStats.UsedPhysical;
	}
	MemoryStats.UsedVirtual = TaskInfo.virtual_size;
	if(MemoryStats.UsedVirtual > MemoryStats.PeakUsedVirtual)
	{
		MemoryStats.PeakUsedVirtual = MemoryStats.UsedVirtual;
	}
	
	return MemoryStats;
}

const FPlatformMemoryConstants& FApplePlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;
	
	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory constants.
		
		// Get page size.
		vm_size_t PageSize;
		host_page_size(mach_host_self(), &PageSize);
		
		// Get memory.
		int64 AvailablePhysical = 0;
		int Mib[] = {CTL_HW, HW_MEMSIZE};
		size_t Length = sizeof(int64);
		sysctl(Mib, 2, &AvailablePhysical, &Length, NULL, 0);
		
		MemoryConstants.TotalPhysical = AvailablePhysical;
		MemoryConstants.TotalVirtual = AvailablePhysical;
		MemoryConstants.PageSize = (uint32)PageSize;
		MemoryConstants.OsAllocationGranularity = (uint32)PageSize;
		MemoryConstants.BinnedPageSize = FMath::Max((SIZE_T)65536, (SIZE_T)PageSize);
		
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}
	
	return MemoryConstants;
}

bool FApplePlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
{
	int32 ProtectMode;
	if (bCanRead && bCanWrite)
	{
		ProtectMode = PROT_READ | PROT_WRITE;
	}
	else if (bCanRead)
	{
		ProtectMode = PROT_READ;
	}
	else if (bCanWrite)
	{
		ProtectMode = PROT_WRITE;
	}
	else
	{
		ProtectMode = PROT_NONE;
	}
	return mprotect(Ptr, Size, static_cast<int32>(ProtectMode)) == 0;
}

void* FApplePlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
// Binned2 requires allocations to be BinnedPageSize-aligned. Simple mmap() does not guarantee this for recommended BinnedPageSize (64KB).
#if USE_MALLOC_BINNED2
    return FGenericPlatformMemory::BinnedAllocFromOS(Size);
#else
    return mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif // USE_MALLOC_BINNED2
}

void FApplePlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
// Binned2 requires allocations to be BinnedPageSize-aligned. Simple mmap() does not guarantee this for recommended BinnedPageSize (64KB).
#if USE_MALLOC_BINNED2
    return FGenericPlatformMemory::BinnedFreeToOS(Ptr, Size);
#else
    if (munmap(Ptr, Size) != 0)
    {
        const int ErrNo = errno;
        UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
               ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
    }
#endif // USE_MALLOC_BINNED2
}

NS_ASSUME_NONNULL_END
