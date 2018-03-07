// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidMemory.h"
#include "MallocBinned.h"
#include "MallocAnsi.h"
#include "unistd.h"
#include <jni.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>

#define JNI_CURRENT_VERSION JNI_VERSION_1_6
extern JavaVM* GJavaVM;

static int64 GetNativeHeapAllocatedSize()
{
	int64 AllocatedSize = 0;
#if 0 // TODO: this works but sometimes crashes?
	JNIEnv* Env = NULL;
	GJavaVM->GetEnv((void **)&Env, JNI_CURRENT_VERSION);
	jint AttachThreadResult = GJavaVM->AttachCurrentThread(&Env, NULL);

	if(AttachThreadResult != JNI_ERR)
	{
		jclass Class = Env->FindClass("android/os/Debug");
		if (Class)
		{
			jmethodID MethodID = Env->GetStaticMethodID(Class, "getNativeHeapAllocatedSize", "()J");
			if (MethodID)
			{
				AllocatedSize = Env->CallStaticLongMethod(Class, MethodID);
			}
		}
	}
#endif
	return AllocatedSize;
}

void FAndroidPlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	FPlatformMemoryStats MemoryStats = GetStats();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.2fMB (%dGB approx) Available=%.2fMB PageSize=%.1fKB"), 
		float(MemoryConstants.TotalPhysical/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB, 
		float(MemoryStats.AvailablePhysical/1024.0/1024.0),
		float(MemoryConstants.PageSize/1024.0)
		);
}

namespace AndroidPlatformMemory
{
	/**
	* @brief Returns value in bytes from a status line
	* @param Line in format "Blah:  10000 kB" - needs to be writable as it will modify it
	* @return value in bytes (10240000, i.e. 10000 * 1024 for the above example)
	*/
	uint64 GetBytesFromStatusLine(char * Line)
	{
		check(Line);
		int Len = strlen(Line);

		// Len should be long enough to hold at least " kB\n"
		const int kSuffixLength = 4;	// " kB\n"
		if (Len <= kSuffixLength)
		{
			return 0;
		}

		// let's check that this is indeed "kB"
		char * Suffix = &Line[Len - kSuffixLength];
		if (strcmp(Suffix, " kB\n") != 0)
		{
			// Linux the kernel changed the format, huh?
			return 0;
		}

		// kill the kB
		*Suffix = 0;

		// find the beginning of the number
		for (const char * NumberBegin = Suffix; NumberBegin >= Line; --NumberBegin)
		{
			if (*NumberBegin == ' ')
			{
				return static_cast< uint64 >(atol(NumberBegin + 1)) * 1024ULL;
			}
		}

		// we were unable to find whitespace in front of the number
		return 0;
	}
}

#if 0

FPlatformMemoryStats FAndroidPlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

	FPlatformMemoryStats MemoryStats;

	//int32 NumAvailPhysPages = sysconf(_SC_AVPHYS_PAGES);
	//MemoryStats.AvailablePhysical = NumAvailPhysPages * MemoryConstants.PageSize;

	MemoryStats.AvailablePhysical = MemoryConstants.TotalPhysical - GetNativeHeapAllocatedSize();
	MemoryStats.AvailableVirtual = 0;
	MemoryStats.UsedPhysical = 0;
	MemoryStats.UsedVirtual = 0;

	return MemoryStats;
}

const FPlatformMemoryConstants& FAndroidPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if (MemoryConstants.TotalPhysical == 0)
	{
		int32 NumPhysPages = sysconf(_SC_PHYS_PAGES);
		int32 PageSize = sysconf(_SC_PAGESIZE);

		MemoryConstants.TotalPhysical = NumPhysPages * PageSize;
		MemoryConstants.TotalVirtual = 0;
		MemoryConstants.PageSize = (uint32)PageSize;

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;
}

#else

FPlatformMemoryStats FAndroidPlatformMemory::GetStats()
{
	FPlatformMemoryStats MemoryStats;	// will init from constants

										// open to all kind of overflows, thanks to Linux approach of exposing system stats via /proc and lack of proper C API
										// And no, sysinfo() isn't useful for this (cannot get the same value for MemAvailable through it for example).

	if (FILE* FileGlobalMemStats = fopen("/proc/meminfo", "r"))
	{
		int FieldsSetSuccessfully = 0;
		uint64 MemFree = 0, Cached = 0;
		do
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, ARRAY_COUNT(LineBuffer), FileGlobalMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			// if we have MemAvailable, favor that (see http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=34e431b0ae398fc54ea69ff85ec700722c9da773)
			if (strstr(Line, "MemAvailable:") == Line)
			{
				MemoryStats.AvailablePhysical = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "SwapFree:") == Line)
			{
				MemoryStats.AvailableVirtual = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "MemFree:") == Line)
			{
				MemFree = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "Cached:") == Line)
			{
				Cached = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		} while (FieldsSetSuccessfully < 4);

		// if we didn't have MemAvailable (kernels < 3.14 or CentOS 6.x), use free + cached as a (bad) approximation
		if (MemoryStats.AvailablePhysical == 0)
		{
			MemoryStats.AvailablePhysical = FMath::Min(MemFree + Cached, MemoryStats.TotalPhysical);
		}

		fclose(FileGlobalMemStats);
	}

	// again /proc "API" :/
	if (FILE* ProcMemStats = fopen("/proc/self/status", "r"))
	{
		int FieldsSetSuccessfully = 0;
		do
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, ARRAY_COUNT(LineBuffer), ProcMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			if (strstr(Line, "VmPeak:") == Line)
			{
				MemoryStats.PeakUsedVirtual = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmSize:") == Line)
			{
				MemoryStats.UsedVirtual = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmHWM:") == Line)
			{
				MemoryStats.PeakUsedPhysical = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmRSS:") == Line)
			{
				MemoryStats.UsedPhysical = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		} while (FieldsSetSuccessfully < 4);

		fclose(ProcMemStats);
	}

	// sanitize stats as sometimes peak < used for some reason
	MemoryStats.PeakUsedVirtual = FMath::Max(MemoryStats.PeakUsedVirtual, MemoryStats.UsedVirtual);
	MemoryStats.PeakUsedPhysical = FMath::Max(MemoryStats.PeakUsedPhysical, MemoryStats.UsedPhysical);

	return MemoryStats;
}

const FPlatformMemoryConstants& FAndroidPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if (MemoryConstants.TotalPhysical == 0)
	{
		// Gather platform memory stats.
		struct sysinfo SysInfo;
		unsigned long long MaxPhysicalRAMBytes = 0;
		unsigned long long MaxVirtualRAMBytes = 0;

		if (0 == sysinfo(&SysInfo))
		{
			MaxPhysicalRAMBytes = static_cast< unsigned long long >(SysInfo.mem_unit) * static_cast< unsigned long long >(SysInfo.totalram);
			MaxVirtualRAMBytes = static_cast< unsigned long long >(SysInfo.mem_unit) * static_cast< unsigned long long >(SysInfo.totalswap);
		}

		MemoryConstants.TotalPhysical = MaxPhysicalRAMBytes;
		MemoryConstants.TotalVirtual = MaxVirtualRAMBytes;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
		MemoryConstants.PageSize = sysconf(_SC_PAGESIZE);
		MemoryConstants.BinnedPageSize = FMath::Max((SIZE_T)65536, MemoryConstants.PageSize);
		MemoryConstants.OsAllocationGranularity = MemoryConstants.PageSize;
	}

	return MemoryConstants;
}

#endif

FMalloc* FAndroidPlatformMemory::BaseAllocator()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	// 1 << FMath::CeilLogTwo(MemoryConstants.TotalPhysical) should really be FMath::RoundUpToPowerOfTwo,
	// but that overflows to 0 when MemoryConstants.TotalPhysical is close to 4GB, since CeilLogTwo returns 32
	// this then causes the MemoryLimit to be 0 and crashing the app
	uint64 MemoryLimit = FMath::Min<uint64>( uint64(1) << FMath::CeilLogTwo(MemoryConstants.TotalPhysical), 0x100000000);

#if PLATFORM_ANDROID_ARM64
	// todo: track down why FMallocBinned is failing on ARM64
	return new FMallocAnsi();
#else
	// [RCL] 2017-03-06 FIXME: perhaps BinnedPageSize should be used here, but leaving this change to the Android platform owner.
	return new FMallocBinned(MemoryConstants.PageSize, MemoryLimit);
#endif
}

void* FAndroidPlatformMemory::BinnedAllocFromOS(SIZE_T Size)
{
	return mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void FAndroidPlatformMemory::BinnedFreeToOS(void* Ptr, SIZE_T Size)
{
	if (munmap(Ptr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}
