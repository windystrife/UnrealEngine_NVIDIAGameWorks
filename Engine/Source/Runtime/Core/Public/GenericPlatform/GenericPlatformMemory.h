// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformMemory.h: Generic platform memory classes
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"
#include <wchar.h>
#include <string.h>

struct FPlatformMemoryStats;

/** Holds generic memory stats, internally implemented as a map. */
struct FGenericMemoryStats;

/** 
 * Struct used to hold common memory constants for all platforms.
 * These values don't change over the entire life of the executable.
 */
struct FGenericPlatformMemoryConstants
{
	/** The amount of actual physical memory, in bytes (needs to handle >4GB for 64-bit devices running 32-bit code). */
	uint64 TotalPhysical;

	/** The amount of virtual memory, in bytes. */
	uint64 TotalVirtual;

	/** The size of a physical page, in bytes. This is also the granularity for PageProtect(), commitment and properties (e.g. ability to access) of the physical RAM. */
	SIZE_T PageSize;

	/**
	 * Some platforms have advantages if memory is allocated in chunks larger than PageSize (e.g. VirtualAlloc() seems to have 64KB granularity as of now).
     * This value is the minimum allocation size that the system will use behind the scenes.
	 */
	SIZE_T OsAllocationGranularity;

	/** The size of a "page" in Binned2 malloc terms, in bytes. Should be at least 64KB. BinnedMalloc expects memory returned from BinnedAllocFromOS() to be aligned on BinnedPageSize boundary. */
	SIZE_T BinnedPageSize;

	/** This is the "allocation granularity" in Binned malloc terms, i.e. BinnedMalloc will allocate the memory in increments of this value. If non-zero, should be at least 16KB. If zero, Binned will use BinnedPageSize for this value. */
	SIZE_T BinnedAllocationGranularity;

	// AddressLimit - Second parameter is estimate of the range of addresses expected to be returns by BinnedAllocFromOS(). Binned
	// Malloc will adjust its internal structures to make lookups for memory allocations O(1) for this range. 
	// It is ok to go outside this range, lookups will just be a little slower
	uint64 AddressLimit;

	/** Approximate physical RAM in GB; 1 on everything except PC. Used for "course tuning", like FPlatformMisc::NumberOfCores(). */
	uint32 TotalPhysicalGB;

	/** Default constructor, clears all variables. */
	FGenericPlatformMemoryConstants()
		: TotalPhysical( 0 )
		, TotalVirtual( 0 )
		, PageSize( 0 )
		, OsAllocationGranularity(0)
		, BinnedPageSize( 0 )
		, BinnedAllocationGranularity( 0 )
		, AddressLimit((uint64)0xffffffff + 1)
		, TotalPhysicalGB( 1 )
	{}

	/** Copy constructor, used by the generic platform memory stats. */
	FGenericPlatformMemoryConstants( const FGenericPlatformMemoryConstants& Other )
		: TotalPhysical( Other.TotalPhysical )
		, TotalVirtual( Other.TotalVirtual )
		, PageSize( Other.PageSize )
		, OsAllocationGranularity(Other.OsAllocationGranularity)
		, BinnedPageSize(Other.BinnedPageSize)
		, BinnedAllocationGranularity(Other.BinnedAllocationGranularity)
		, AddressLimit(Other.AddressLimit)
		, TotalPhysicalGB(Other.TotalPhysicalGB)
	{}
};

typedef FGenericPlatformMemoryConstants FPlatformMemoryConstants;

/** 
 * Struct used to hold common memory stats for all platforms.
 * These values may change over the entire life of the executable.
 */
struct FGenericPlatformMemoryStats : public FPlatformMemoryConstants
{
	/** The amount of physical memory currently available, in bytes. */
	uint64 AvailablePhysical;

	/** The amount of virtual memory currently available, in bytes. */
	uint64 AvailableVirtual;

	/** The amount of physical memory used by the process, in bytes. */
	uint64 UsedPhysical;

	/** The peak amount of physical memory used by the process, in bytes. */
	uint64 PeakUsedPhysical;

	/** Total amount of virtual memory used by the process. */
	uint64 UsedVirtual;

	/** The peak amount of virtual memory used by the process. */
	uint64 PeakUsedVirtual;
	
	/** Default constructor, clears all variables. */
	FGenericPlatformMemoryStats();
};

struct FPlatformMemoryStats;

/**
 * FMemory_Alloca/alloca implementation. This can't be a function, even FORCEINLINE'd because there's no guarantee that 
 * the memory returned in a function will stick around for the caller to use.
 */
#if PLATFORM_USES_MICROSOFT_LIBC_FUNCTIONS
#define __FMemory_Alloca_Func _alloca
#else
#define __FMemory_Alloca_Func alloca
#endif

#define FMemory_Alloca(Size )((Size==0) ? 0 : (void*)(((PTRINT)__FMemory_Alloca_Func(Size + 15) + 15) & ~15))

/** Generic implementation for most platforms, these tend to be unused and unimplemented. */
struct CORE_API FGenericPlatformMemory
{
	/** Set to true if we encounters out of memory. */
	static bool bIsOOM;

	/** Set to size of allocation that triggered out of memory, zero otherwise. */
	static uint64 OOMAllocationSize;

	/** Set to alignment of allocation that triggered out of memory, zero otherwise. */
	static uint32 OOMAllocationAlignment;

	/** Preallocated buffer to delete on out of memory. Used by OOM handling and crash reporting. */
	static void* BackupOOMMemoryPool;

	/** Size of BackupOOMMemoryPool in bytes. */
	static uint32 BackupOOMMemoryPoolSize;

	/**
	 * Various memory regions that can be used with memory stats. The exact meaning of
	 * the enums are relatively platform-dependent, although the general ones (Physical, GPU)
	 * are straightforward. A platform can add more of these, and it won't affect other 
	 * platforms, other than a minuscule amount of memory for the StatManager to track the
	 * max available memory for each region (uses an array FPlatformMemory::MCR_MAX big)
	 */
	enum EMemoryCounterRegion
	{
		MCR_Invalid, // not memory
		MCR_Physical, // main system memory
		MCR_GPU, // memory directly a GPU (graphics card, etc)
		MCR_GPUSystem, // system memory directly accessible by a GPU
		MCR_TexturePool, // presized texture pools
		MCR_StreamingPool, // amount of texture pool available for streaming.
		MCR_UsedStreamingPool, // amount of texture pool used for streaming.
		MCR_GPUDefragPool, // presized pool of memory that can be defragmented.
		MCR_PhysicalLLM, // total physical memory including CPU and GPU
		MCR_MAX
	};

	/** Which allocator is being used */
	enum EMemoryAllocatorToUse
	{
		Ansi, // Default C allocator
		Stomp, // Allocator to check for memory stomping
		TBB, // Thread Building Blocks malloc
		Jemalloc, // Linux/FreeBSD malloc
		Binned, // Older binned malloc
		Binned2, // Newer binned malloc
		Platform, // Custom platform specific allocator
	};

	/** Current allocator */
	static EMemoryAllocatorToUse AllocatorToUse;

	/**
	 * Flags used for shared memory creation/open
	 */
	enum ESharedMemoryAccess
	{
		Read	=		(1 << 1),
		Write	=		(1 << 2)
	};

	/**
	 * Generic representation of a shared memory region
	 */
	struct FSharedMemoryRegion
	{
		/** Returns the name of the region */
		const TCHAR *	GetName() const			{ return Name; }

		/** Returns the beginning of the region in process address space */
		void *			GetAddress()			{ return Address; }

		/** Returns the beginning of the region in process address space */
		const void *	GetAddress() const		{ return Address; }

		/** Returns size of the region in bytes */
		SIZE_T			GetSize() const			{ return Size; }
	
		
		FSharedMemoryRegion(const FString& InName, uint32 InAccessMode, void* InAddress, SIZE_T InSize);

	protected:

		enum Limits
		{
			MaxSharedMemoryName		=	128
		};

		/** Name of the region */
		TCHAR			Name[MaxSharedMemoryName];

		/** Access mode for the region */
		uint32			AccessMode;

		/** The actual buffer */
		void *			Address;

		/** Size of the buffer */
		SIZE_T			Size;
	};

	/** Initializes platform memory specific constants. */
	static void Init();
	
	static CA_NO_RETURN void OnOutOfMemory(uint64 Size, uint32 Alignment);

	/** Initializes the memory pools, should be called by the init function. */
	static void SetupMemoryPools();

	
	/**
	 * @return how much memory the platform should pre-allocate for crash handling (this will be allocated ahead of time, and freed when system runs out of memory).
	 */
	static uint32 GetBackMemoryPoolSize()
	{
		// by default, don't create a backup memory buffer
		return 0;
	}

	/**
	 * @return the default allocator.
	 */
	static FMalloc* BaseAllocator();

	/**
	 * @return platform specific current memory statistics.
	 */
	static FPlatformMemoryStats GetStats();

	/**
	 * Writes all platform specific current memory statistics in the format usable by the malloc profiler.
	 */
	static void GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats );

	/**
	 * @return platform specific memory constants.
	 */
	static const FPlatformMemoryConstants& GetConstants();
	
	/**
	 * @return approximate physical RAM in GB.
	 */
	static uint32 GetPhysicalGBRam();

	/**
	 * Changes the protection on a region of committed pages in the virtual address space.
	 *
	 * @param Ptr Address to the starting page of the region of pages whose access protection attributes are to be changed.
	 * @param Size The size of the region whose access protection attributes are to be changed, in bytes.
	 * @param bCanRead Can the memory be read.
	 * @param bCanWrite Can the memory be written to.
	 * @return True if the specified pages' protection mode was changed.
	 */
	static bool PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite);

	/**
	 * Allocates pages from the OS.
	 *
	 * @param Size Size to allocate, not necessarily aligned
	 *
	 * @return OS allocated pointer for use by binned allocator
	 */
	static void* BinnedAllocFromOS( SIZE_T Size );
	
	/**
	 * Returns pages allocated by BinnedAllocFromOS to the OS.
	 *
	 * @param A pointer previously returned from BinnedAllocFromOS
	 * @param Size size of the allocation previously passed to BinnedAllocFromOS
	 */
	static void BinnedFreeToOS( void* Ptr, SIZE_T Size );

	/**
	 * Some platforms may pool allocations of this size to reduce OS calls. This function
	 * serves as a hint for BinnedMalloc's CachedOSPageAllocator so it does not cache these allocations additionally
	 */
	static bool BinnedPlatformHasMemoryPoolForThisSize(SIZE_T Size)
	{
		return false;
	}

	// These alloc/free memory that is mapped to the GPU
	// Only for platforms with UMA (XB1/PS4/etc)
	static void* GPUMalloc(SIZE_T Count, uint32 Alignment = 0) { return nullptr; };
	static void* GPURealloc(void* Original, SIZE_T Count, uint32 Alignment = 0) { return nullptr; };
	static void GPUFree(void* Original) { };

	/** Dumps basic platform memory statistics into the specified output device. */
	static void DumpStats( FOutputDevice& Ar );

	/** Dumps basic platform memory statistics and allocator specific statistics into the specified output device. */
	static void DumpPlatformAndAllocatorStats( FOutputDevice& Ar );

	/** @name Memory functions */

	/** Copies count bytes of characters from Src to Dest. If some regions of the source
	 * area and the destination overlap, memmove ensures that the original source bytes
	 * in the overlapping region are copied before being overwritten.  NOTE: make sure
	 * that the destination buffer is the same size or larger than the source buffer!
	 */
	static FORCEINLINE void* Memmove( void* Dest, const void* Src, SIZE_T Count )
	{
		return memmove( Dest, Src, Count );
	}

	static FORCEINLINE int32 Memcmp( const void* Buf1, const void* Buf2, SIZE_T Count )
	{
		return memcmp( Buf1, Buf2, Count );
	}

	static FORCEINLINE void* Memset(void* Dest, uint8 Char, SIZE_T Count)
	{
		return memset( Dest, Char, Count );
	}

	static FORCEINLINE void* Memzero(void* Dest, SIZE_T Count)
	{
		return memset( Dest, 0, Count );
	}

	static FORCEINLINE void* Memcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return memcpy( Dest, Src, Count );
	}

	/** Memcpy optimized for big blocks. */
	static FORCEINLINE void* BigBlockMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return memcpy(Dest, Src, Count);
	}

	/** On some platforms memcpy optimized for big blocks that avoid L2 cache pollution are available */
	static FORCEINLINE void* StreamingMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return memcpy( Dest, Src, Count );
	}

private:
	template <typename T>
	static FORCEINLINE void Valswap(T& A, T& B)
	{
		// Usually such an implementation would use move semantics, but
		// we're only ever going to call it on fundamental types and MoveTemp
		// is not necessarily in scope here anyway, so we don't want to
		// #include it if we don't need to.
		T Tmp = A;
		A = B;
		B = Tmp;
	}

	static void MemswapGreaterThan8( void* Ptr1, void* Ptr2, SIZE_T Size );

public:
	static inline void Memswap( void* Ptr1, void* Ptr2, SIZE_T Size )
	{
		switch (Size)
		{
			case 0:
				break;

			case 1:
				Valswap(*(uint8*)Ptr1, *(uint8*)Ptr2);
				break;

			case 2:
				Valswap(*(uint16*)Ptr1, *(uint16*)Ptr2);
				break;

			case 3:
				Valswap(*((uint16*&)Ptr1)++, *((uint16*&)Ptr2)++);
				Valswap(*(uint8*)Ptr1, *(uint8*)Ptr2);
				break;

			case 4:
				Valswap(*(uint32*)Ptr1, *(uint32*)Ptr2);
				break;

			case 5:
				Valswap(*((uint32*&)Ptr1)++, *((uint32*&)Ptr2)++);
				Valswap(*(uint8*)Ptr1, *(uint8*)Ptr2);
				break;

			case 6:
				Valswap(*((uint32*&)Ptr1)++, *((uint32*&)Ptr2)++);
				Valswap(*(uint16*)Ptr1, *(uint16*)Ptr2);
				break;

			case 7:
				Valswap(*((uint32*&)Ptr1)++, *((uint32*&)Ptr2)++);
				Valswap(*((uint16*&)Ptr1)++, *((uint16*&)Ptr2)++);
				Valswap(*(uint8*)Ptr1, *(uint8*)Ptr2);
				break;

			case 8:
				Valswap(*(uint64*)Ptr1, *(uint64*)Ptr2);
				break;

			case 16:
				Valswap(((uint64*)Ptr1)[0], ((uint64*)Ptr2)[0]);
				Valswap(((uint64*)Ptr1)[1], ((uint64*)Ptr2)[1]);
				break;

			default:
				MemswapGreaterThan8(Ptr1, Ptr2, Size);
				break;
		}
	}

	/**
	 * Maps a named shared memory region into process address space (creates or opens it)
	 *
	 * @param Name unique name of the shared memory region (should not contain [back]slashes to remain cross-platform).
	 * @param bCreate whether we're creating it or just opening existing (created by some other process).
	 * @param AccessMode mode which we will be accessing it (use values from ESharedMemoryAccess)
	 * @param Size size of the buffer (should be >0. Also, the real size is subject to platform limitations and may be increased to match page size)
	 *
	 * @return pointer to FSharedMemoryRegion (or its descendants) if successful, NULL if not.
	 */
	static FSharedMemoryRegion* MapNamedSharedMemoryRegion(const FString& Name, bool bCreate, uint32 AccessMode, SIZE_T Size);

	/**
	 * Unmaps a name shared memory region
	 *
	 * @param MemoryRegion an object that encapsulates a shared memory region (will be destroyed even if function fails!)
	 *
	 * @return true if successful
	 */
	static bool UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion);

	/**
	*	Gets whether this platform supports Fast VRAM memory
	*		Ie, whether TexCreate_FastVRAM flags actually mean something or not
	*
	*	@return	bool		true if supported, false if not
	*/
	static FORCEINLINE bool SupportsFastVRAMMemory()
	{
		return false;
	}

	/**
	* Returns true if debug memory has been assigned to the title for general use.
	* Only applies to consoles with fixed memory and no paging.
	* On XB1 set Debug Memory Mode to PIX_Title. On PS4 set Memory Budget Mode to LARGE.
	*/
	static bool IsDebugMemoryEnabled();

	/**
	* This function sets AllocFunction and FreeFunction and returns true, or just returns false.
	* These functions are the platform dependant low low low level functions that LLM uses to allocate memory.
	*/
	static bool GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment);

protected:
	friend struct FGenericStatsUpdater;

	/** Updates platform specific stats. This method is called through FGenericStatsUpdater from the task graph thread. */
	static void InternalUpdateStats( const FPlatformMemoryStats& MemoryStats );

};
