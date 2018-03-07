// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericPlatformMisc.cpp: Generic implementations of misc platform functions
=============================================================================*/

#include "Linux/LinuxPlatformMisc.h"
#include "Misc/AssertionMacros.h"
#include "GenericPlatform/GenericPlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Linux/LinuxPlatformCrashContext.h"

#if PLATFORM_HAS_CPUID
	#include <cpuid.h>
#endif // PLATFORM_HAS_CPUID
#include <sys/sysinfo.h>
#include <sched.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>

#include "Modules/ModuleManager.h"
#include "HAL/ThreadHeartBeat.h"

// define for glibc 2.12.2 and lower (which is shipped with CentOS 6.x and which we target by default)
#define __secure_getenv getenv

extern bool GInitializedSDL;

namespace PlatformMiscLimits
{
	enum
	{
		MaxOsGuidLength = 32
	};
};

namespace
{
	/**
	 * Empty handler so some signals are just not ignored
	 */
	void EmptyChildHandler(int32 Signal, siginfo_t* Info, void* Context)
	{
	}

	/**
	 * Installs SIGCHLD signal handler so we can wait for our children (otherwise they are reaped automatically)
	 */
	void InstallChildExitedSignalHanlder()
	{
		struct sigaction Action;
		FMemory::Memzero(Action);
		Action.sa_sigaction = EmptyChildHandler;
		sigfillset(&Action.sa_mask);
		Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
		sigaction(SIGCHLD, &Action, nullptr);
	}
}

void FLinuxPlatformMisc::NormalizePath(FString& InPath)
{
	// only expand if path starts with ~, e.g. ~/ should be expanded, /~ should not
	if (InPath.StartsWith(TEXT("~"), ESearchCase::CaseSensitive))	// case sensitive is quicker, and our substring doesn't care
	{
		InPath = InPath.Replace(TEXT("~"), FPlatformProcess::UserHomeDir(), ESearchCase::CaseSensitive);
	}
}

size_t CORE_API GCacheLineSize = PLATFORM_CACHE_LINE_SIZE;

void LinuxPlatform_UpdateCacheLineSize()
{
	// sysfs "API", as usual ;/
	FILE * SysFsFile = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
	if (SysFsFile)
	{
		int SystemLineSize = 0;
		if (1 == fscanf(SysFsFile, "%d", &SystemLineSize))
		{
			if (SystemLineSize > 0)
			{
				GCacheLineSize = SystemLineSize;
			}
		}
		fclose(SysFsFile);
	}
}

void FLinuxPlatformMisc::PlatformInit()
{
	// install a platform-specific signal handler
	InstallChildExitedSignalHanlder();

	// do not remove the below check for IsFirstInstance() - it is not just for logging, it actually lays the claim to be first
	bool bFirstInstance = FPlatformProcess::IsFirstInstance();
	bool bIsNullRHI = !FApp::CanEverRender();

	UE_LOG(LogInit, Log, TEXT("Linux hardware info:"));
	UE_LOG(LogInit, Log, TEXT(" - we are %sthe first instance of this executable"), bFirstInstance ? TEXT("") : TEXT("not "));
	UE_LOG(LogInit, Log, TEXT(" - this process' id (pid) is %d, parent process' id (ppid) is %d"), static_cast< int32 >(getpid()), static_cast< int32 >(getppid()));
	UE_LOG(LogInit, Log, TEXT(" - we are %srunning under debugger"), IsDebuggerPresent() ? TEXT("") : TEXT("not "));
	UE_LOG(LogInit, Log, TEXT(" - machine network name is '%s'"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT(" - user name is '%s' (%s)"), FPlatformProcess::UserName(), FPlatformProcess::UserName(false));
	UE_LOG(LogInit, Log, TEXT(" - we're logged in %s"), FPlatformMisc::HasBeenStartedRemotely() ? TEXT("remotely") : TEXT("locally"));
	UE_LOG(LogInit, Log, TEXT(" - we're running %s rendering"), bIsNullRHI ? TEXT("without") : TEXT("with"));
	UE_LOG(LogInit, Log, TEXT(" - CPU: %s '%s' (signature: 0x%X)"), *FPlatformMisc::GetCPUVendor(), *FPlatformMisc::GetCPUBrand(), FPlatformMisc::GetCPUInfo());
	UE_LOG(LogInit, Log, TEXT(" - Number of physical cores available for the process: %d"), FPlatformMisc::NumberOfCores());
	UE_LOG(LogInit, Log, TEXT(" - Number of logical cores available for the process: %d"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	LinuxPlatform_UpdateCacheLineSize();
	UE_LOG(LogInit, Log, TEXT(" - Cache line size: %Zu"), GCacheLineSize);
	UE_LOG(LogInit, Log, TEXT(" - Memory allocator used: %s"), GMalloc->GetDescriptiveName());

	FPlatformTime::PrintCalibrationLog();

	UE_LOG(LogInit, Log, TEXT("Linux-specific commandline switches:"));
	UE_LOG(LogInit, Log, TEXT(" -%s (currently %s): suppress parsing of DWARF debug info (callstacks will be generated faster, but won't have line numbers)"), 
		TEXT(CMDARG_SUPPRESS_DWARF_PARSING), FParse::Param( FCommandLine::Get(), TEXT(CMDARG_SUPPRESS_DWARF_PARSING)) ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogInit, Log, TEXT(" -ansimalloc - use malloc()/free() from libc (useful for tools like valgrind and electric fence)"));
	UE_LOG(LogInit, Log, TEXT(" -jemalloc - use jemalloc for all memory allocation"));
	UE_LOG(LogInit, Log, TEXT(" -binnedmalloc - use binned malloc  for all memory allocation"));

	// [RCL] FIXME: this should be printed in specific modules, if at all
	UE_LOG(LogInit, Log, TEXT(" -httpproxy=ADDRESS:PORT - redirects HTTP requests to a proxy (only supported if compiled with libcurl)"));
	UE_LOG(LogInit, Log, TEXT(" -reuseconn - allow libcurl to reuse HTTP connections (only matters if compiled with libcurl)"));
	UE_LOG(LogInit, Log, TEXT(" -virtmemkb=NUMBER - sets process virtual memory (address space) limit (overrides VirtualMemoryLimitInKB value from .ini)"));

	if (FPlatformMisc::HasBeenStartedRemotely() || FPlatformMisc::IsDebuggerPresent())
	{
		// print output immediately
		setvbuf(stdout, NULL, _IONBF, 0);
	}
}

void FLinuxPlatformMisc::PlatformTearDown()
{
	FPlatformProcess::CeaseBeingFirstInstance();
}

void FLinuxPlatformMisc::GetEnvironmentVariable(const TCHAR* InVariableName, TCHAR* Result, int32 ResultLength)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	ANSICHAR *AnsiResult = secure_getenv(TCHAR_TO_ANSI(*VariableName));
	if (AnsiResult)
	{
		wcsncpy(Result, UTF8_TO_TCHAR(AnsiResult), ResultLength);
	}
	else
	{
		*Result = 0;
	}
}

void FLinuxPlatformMisc::SetEnvironmentVar(const TCHAR* InVariableName, const TCHAR* Value)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	if (Value == NULL || Value[0] == TEXT('\0'))
	{
		unsetenv(TCHAR_TO_ANSI(*VariableName));
	}
	else
	{
		setenv(TCHAR_TO_ANSI(*VariableName), TCHAR_TO_ANSI(Value), 1);
	}
}

void FLinuxPlatformMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	static_assert(PLATFORM_USE_LS_SPEC_FOR_WIDECHAR, "Check printf format");
	fprintf(stderr, "%ls", Message);	// there's no good way to implement that really
}

uint8 GOverriddenReturnCode = 0;
bool GHasOverriddenReturnCode = false;

void FLinuxPlatformMisc::RequestExit(bool Force)
{
	UE_LOG(LogLinux, Log,  TEXT("FLinuxPlatformMisc::RequestExit(%i)"), Force );
	if( Force )
	{
		// Force immediate exit. Cannot call abort() here, because abort() raises SIGABRT which we treat as crash
		// (to prevent other, particularly third party libs, from quitting without us noticing)
		// Propagate override return code, but normally don't exit with 0, so the parent knows it wasn't a normal exit.
		if (GHasOverriddenReturnCode)
		{
			_exit(GOverriddenReturnCode);
		}
		else
		{
			_exit(1);
		}
	}

	// Tell the platform specific code we want to exit cleanly from the main loop.
	FGenericPlatformMisc::RequestExit(Force);
}

void FLinuxPlatformMisc::RequestExitWithStatus(bool Force, uint8 ReturnCode)
{
	UE_LOG(LogLinux, Log, TEXT("FLinuxPlatformMisc::RequestExit(bForce=%s, ReturnCode=%d)"), Force ? TEXT("true") : TEXT("false"), ReturnCode);

	GOverriddenReturnCode = ReturnCode;
	GHasOverriddenReturnCode = true;

	return FPlatformMisc::RequestExit(Force);
}

bool FLinuxPlatformMisc::HasOverriddenReturnCode(uint8 * OverriddenReturnCodeToUsePtr)
{
	if (GHasOverriddenReturnCode && OverriddenReturnCodeToUsePtr != nullptr)
	{
		*OverriddenReturnCodeToUsePtr = GOverriddenReturnCode;
	}

	return GHasOverriddenReturnCode;
}

FString FLinuxPlatformMisc::GetOSVersion()
{
	// TODO [RCL] 2015-07-15: check if /etc/os-release or /etc/redhat-release exist and parse it
	// See void FLinuxPlatformSurvey::GetOSName(FHardwareSurveyResults& OutResults)
	return FString();
}

const TCHAR* FLinuxPlatformMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	if (Error == 0)
	{
		Error = errno;
	}

	FString Message = FString::Printf(TEXT("errno=%d (%s)"), Error, UTF8_TO_TCHAR(strerror(Error)));
	FCString::Strncpy(OutBuffer, *Message, BufferCount);

	return OutBuffer;
}

CORE_API TFunction<EAppReturnType::Type(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)> MessageBoxExtCallback;

EAppReturnType::Type FLinuxPlatformMisc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	if(MessageBoxExtCallback)
	{
		return MessageBoxExtCallback(MsgType, Text, Caption);
	}
	else
	{
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}
}

int32 FLinuxPlatformMisc::NumberOfCores()
{
	// WARNING: this function ignores edge cases like affinity mask changes (and even more fringe cases like CPUs going offline)
	// in the name of performance (higher level code calls NumberOfCores() way too often...)
	static int32 NumberOfCores = 0;
	if (NumberOfCores == 0)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("usehyperthreading")))
		{
			NumberOfCores = NumberOfCoresIncludingHyperthreads();
		}
		else
		{
			cpu_set_t AvailableCpusMask;
			CPU_ZERO(&AvailableCpusMask);

			if (0 != sched_getaffinity(0, sizeof(AvailableCpusMask), &AvailableCpusMask))
			{
				NumberOfCores = 1;	// we are running on something, right?
			}
			else
			{
				char FileNameBuffer[1024];
				struct CpuInfo
				{
					int Core;
					int Package;
				}
				CpuInfos[CPU_SETSIZE];

				FMemory::Memzero(CpuInfos);
				int MaxCoreId = 0;
				int MaxPackageId = 0;
				int NumCpusAvailable = 0;

				for(int32 CpuIdx = 0; CpuIdx < CPU_SETSIZE; ++CpuIdx)
				{
					if (CPU_ISSET(CpuIdx, &AvailableCpusMask))
					{
						++NumCpusAvailable;

						sprintf(FileNameBuffer, "/sys/devices/system/cpu/cpu%d/topology/core_id", CpuIdx);

						if (FILE* CoreIdFile = fopen(FileNameBuffer, "r"))
						{
							if (1 != fscanf(CoreIdFile, "%d", &CpuInfos[CpuIdx].Core))
							{
								CpuInfos[CpuIdx].Core = 0;
							}
							fclose(CoreIdFile);
						}

						sprintf(FileNameBuffer, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", CpuIdx);

						if (FILE* PackageIdFile = fopen(FileNameBuffer, "r"))
						{
							// physical_package_id can be -1 on embedded devices - treat all CPUs as separate in that case.
							if (1 != fscanf(PackageIdFile, "%d", &CpuInfos[CpuIdx].Package) || CpuInfos[CpuIdx].Package < 0)
							{
								CpuInfos[CpuIdx].Package = CpuInfos[CpuIdx].Core;
							}
							fclose(PackageIdFile);
						}

						MaxCoreId = FMath::Max(MaxCoreId, CpuInfos[CpuIdx].Core);
						MaxPackageId = FMath::Max(MaxPackageId, CpuInfos[CpuIdx].Package);
					}
				}

				int NumCores = MaxCoreId + 1;
				int NumPackages = MaxPackageId + 1;
				int NumPairs = NumPackages * NumCores;

				// AArch64 topology seems to be incompatible with the above assumptions, particularly, core_id can be all 0 while the cores themselves are obviously independent. 
				// Check if num CPUs available to us is more than 2 per core (i.e. more than reasonable when hyperthreading is involved), and if so, don't trust the topology.
				if (2 * NumCores < NumCpusAvailable)
				{
					NumberOfCores = NumCpusAvailable;	// consider all CPUs to be separate
				}
				else
				{
					unsigned char * Pairs = reinterpret_cast<unsigned char *>(FMemory_Alloca(NumPairs * sizeof(unsigned char)));
					FMemory::Memzero(Pairs, NumPairs * sizeof(unsigned char));

					for (int32 CpuIdx = 0; CpuIdx < CPU_SETSIZE; ++CpuIdx)
					{
						if (CPU_ISSET(CpuIdx, &AvailableCpusMask))
						{
							Pairs[CpuInfos[CpuIdx].Package * NumCores + CpuInfos[CpuIdx].Core] = 1;
						}
					}

					for (int32 Idx = 0; Idx < NumPairs; ++Idx)
					{
						NumberOfCores += Pairs[Idx];
					}
				}
			}
		}

		// never allow it to be less than 1, we are running on something
		NumberOfCores = FMath::Max(1, NumberOfCores);
	}

	return NumberOfCores;
}

int32 FLinuxPlatformMisc::NumberOfCoresIncludingHyperthreads()
{
	// WARNING: this function ignores edge cases like affinity mask changes (and even more fringe cases like CPUs going offline)
	// in the name of performance (higher level code calls NumberOfCores() way too often...)
	static int32 NumCoreIds = 0;
	if (NumCoreIds == 0)
	{
		cpu_set_t AvailableCpusMask;
		CPU_ZERO(&AvailableCpusMask);

		if (0 != sched_getaffinity(0, sizeof(AvailableCpusMask), &AvailableCpusMask))
		{
			NumCoreIds = 1;	// we are running on something, right?
		}
		else
		{
			return CPU_COUNT(&AvailableCpusMask);
		}
	}

	return NumCoreIds;
}

const TCHAR* FLinuxPlatformMisc::GetNullRHIShaderFormat()
{
	return TEXT("GLSL_150");
}

bool FLinuxPlatformMisc::HasCPUIDInstruction()
{
#if PLATFORM_HAS_CPUID
	return __get_cpuid_max(0, 0) != 0;
#else
	return false;	// Linux ARM or something more exotic
#endif // PLATFORM_HAS_CPUID
}

FString FLinuxPlatformMisc::GetCPUVendor()
{
	static TCHAR Result[13] = TEXT("NonX86Vendor");
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
#if PLATFORM_HAS_CPUID
		union
		{
			char Buffer[12 + 1];
			struct
			{
				int dw0;
				int dw1;
				int dw2;
			} Dw;
		} VendorResult;

		int Dummy;
		__cpuid(0, Dummy, VendorResult.Dw.dw0, VendorResult.Dw.dw2, VendorResult.Dw.dw1);

		VendorResult.Buffer[12] = 0;

		FCString::Strncpy(Result, UTF8_TO_TCHAR(VendorResult.Buffer), ARRAY_COUNT(Result));
#else
		// use /proc?
#endif // PLATFORM_HAS_CPUID

		bHaveResult = true;
	}

	return FString(Result);
}

uint32 FLinuxPlatformMisc::GetCPUInfo()
{
	static uint32 Info = 0;
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
#if PLATFORM_HAS_CPUID
		int Dummy[3];
		__cpuid(1, Info, Dummy[0], Dummy[1], Dummy[2]);
#endif // PLATFORM_HAS_CPUID

		bHaveResult = true;
	}

	return Info;
}

FString FLinuxPlatformMisc::GetCPUBrand()
{
	static TCHAR Result[64] = TEXT("NonX86CPUBrand");
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
#if PLATFORM_HAS_CPUID
		// @see for more information http://msdn.microsoft.com/en-us/library/vstudio/hskdteyh(v=vs.100).aspx
		ANSICHAR BrandString[0x40] = { 0 };
		int32 CPUInfo[4] = { -1 };
		const SIZE_T CPUInfoSize = sizeof(CPUInfo);

		__cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
		const uint32 MaxExtIDs = CPUInfo[0];

		if (MaxExtIDs >= 0x80000004)
		{
			const uint32 FirstBrandString = 0x80000002;
			const uint32 NumBrandStrings = 3;
			for (uint32 Index = 0; Index < NumBrandStrings; ++Index)
			{
				__cpuid(FirstBrandString + Index, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
				FPlatformMemory::Memcpy(BrandString + CPUInfoSize * Index, CPUInfo, CPUInfoSize);
			}
		}

		FCString::Strncpy(Result, UTF8_TO_TCHAR(BrandString), ARRAY_COUNT(Result));
#else
		// use /proc?
#endif // PLATFORM_HAS_CPUID

		bHaveResult = true;
	}

	return FString(Result);
}

// __builtin_popcountll() will not be compiled to use popcnt instruction unless -mpopcnt or a sufficiently recent target CPU arch is passed (which UBT doesn't by default)
#if defined(__POPCNT__)
	#define UE4_LINUX_NEED_TO_CHECK_FOR_POPCNT_PRESENCE				(PLATFORM_ENABLE_POPCNT_INTRINSIC)
#else
	#define UE4_LINUX_NEED_TO_CHECK_FOR_POPCNT_PRESENCE				0
#endif // __POPCNT__

bool FLinuxPlatformMisc::HasNonoptionalCPUFeatures()
{
	static bool bHasNonOptionalFeature = false;
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
#if PLATFORM_HAS_CPUID
		int Info[4];
		__cpuid(1, Info[0], Info[1], Info[2], Info[3]);
	
	#if UE4_LINUX_NEED_TO_CHECK_FOR_POPCNT_PRESENCE
		bHasNonOptionalFeature = (Info[2] & (1 << 23)) != 0;
	#endif // UE4_LINUX_NEED_TO_CHECK_FOR_POPCNT
#endif // PLATFORM_HAS_CPUID

		bHaveResult = true;
	}

	return bHasNonOptionalFeature;
}

bool FLinuxPlatformMisc::NeedsNonoptionalCPUFeaturesCheck()
{
#if UE4_LINUX_NEED_TO_CHECK_FOR_POPCNT_PRESENCE
	return true;
#else
	return false;
#endif
}


#if !UE_BUILD_SHIPPING
bool FLinuxPlatformMisc::IsDebuggerPresent()
{
	extern CORE_API bool GIgnoreDebugger;
	if (GIgnoreDebugger)
	{
		return false;
	}

	// If a process is tracing this one then TracerPid in /proc/self/status will
	// be the id of the tracing process. Use SignalHandler safe functions 
	
	int StatusFile = open("/proc/self/status", O_RDONLY);
	if (StatusFile == -1) 
	{
		// Failed - unknown debugger status.
		return false;
	}

	char Buffer[256];	
	ssize_t Length = read(StatusFile, Buffer, sizeof(Buffer));
	
	bool bDebugging = false;
	const char* TracerString = "TracerPid:\t";
	const ssize_t LenTracerString = strlen(TracerString);
	int i = 0;

	while((Length - i) > LenTracerString)
	{
		// TracerPid is found
		if (strncmp(&Buffer[i], TracerString, LenTracerString) == 0)
		{
			// 0 if no process is tracing.
			bDebugging = Buffer[i + LenTracerString] != '0';
			break;
		}

		++i;
	}

	close(StatusFile);
	return bDebugging;
}
#endif // !UE_BUILD_SHIPPING

bool FLinuxPlatformMisc::HasBeenStartedRemotely()
{
	static bool bHaveAnswer = false;
	static bool bResult = false;

	if (!bHaveAnswer)
	{
		const char * VarValue = secure_getenv("SSH_CONNECTION");
		bResult = (VarValue && strlen(VarValue) != 0);
		bHaveAnswer = true;
	}

	return bResult;
}

FString FLinuxPlatformMisc::GetOperatingSystemId()
{
	static bool bHasCachedResult = false;
	static FString CachedResult;

	if (!bHasCachedResult)
	{
		int OsGuidFile = open("/etc/machine-id", O_RDONLY);
		if (OsGuidFile != -1)
		{
			char Buffer[PlatformMiscLimits::MaxOsGuidLength + 1] = {0};
			ssize_t ReadBytes = read(OsGuidFile, Buffer, sizeof(Buffer) - 1);

			if (ReadBytes > 0)
			{
				CachedResult = UTF8_TO_TCHAR(Buffer);
			}

			close(OsGuidFile);
		}

		// old POSIX gethostid() is not useful. It is impossible to have globally unique 32-bit GUIDs and most
		// systems don't try hard implementing it these days (glibc will return a permuted IP address, often 127.0.0.1)
		// Due to that, we just ignore that call and consider lack of systemd's /etc/machine-id a failure to obtain the host id.

		bHasCachedResult = true;	// even if we failed to read the real one
	}

	return CachedResult;
}

bool FLinuxPlatformMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
	struct statfs FSStat = { 0 };
	FTCHARToUTF8 Converter(*InPath);
	int Err = statfs((ANSICHAR*)Converter.Get(), &FSStat);
	if (Err == 0)
	{
		TotalNumberOfBytes = FSStat.f_blocks * FSStat.f_bsize;
		NumberOfFreeBytes = FSStat.f_bavail * FSStat.f_bsize;
	}
	else
	{
		int ErrNo = errno;
		UE_LOG(LogLinux, Warning, TEXT("Unable to statfs('%s'): errno=%d (%s)"), *InPath, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
	}
	return (Err == 0);
}


TArray<uint8> FLinuxPlatformMisc::GetMacAddress()
{
	struct ifaddrs *ifap, *ifaptr;
	TArray<uint8> Result;

	if (getifaddrs(&ifap) == 0)
	{
		for (ifaptr = ifap; ifaptr != nullptr; ifaptr = (ifaptr)->ifa_next)
		{
			struct ifreq ifr;

			strncpy(ifr.ifr_name, ifaptr->ifa_name, IFNAMSIZ-1);

			int Socket = socket(AF_UNIX, SOCK_DGRAM, 0);
			if (Socket == -1)
			{
				continue;
			}

			if (ioctl(Socket, SIOCGIFHWADDR, &ifr) == -1)
			{
				close(Socket);
				continue;
			}

			close(Socket);

			if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
			{
				continue;
			}

			const uint8 *MAC = (uint8 *) ifr.ifr_hwaddr.sa_data;

			for (int32 i=0; i < 6; i++)
			{
				Result.Add(MAC[i]);
			}

			break;
		}

		freeifaddrs(ifap);
	}

	return Result;
}


static int64 LastBatteryCheck = 0;
static bool bIsOnBattery = false;

bool FLinuxPlatformMisc::IsRunningOnBattery()
{
	char Scratch[8];
	FDateTime Time = FDateTime::Now();
	int64 Seconds = Time.ToUnixTimestamp();

	// don't poll the OS for battery state on every tick. Just do it once every 10 seconds.
	if (LastBatteryCheck != 0 && (Seconds - LastBatteryCheck) < 10)
	{
		return bIsOnBattery;
	}

	LastBatteryCheck = Seconds;
	bIsOnBattery = false;

	// [RCL] 2015-09-30 FIXME: find a more robust way?
	const int kHardCodedNumBatteries = 10;
	for (int IdxBattery = 0; IdxBattery < kHardCodedNumBatteries; ++IdxBattery)
	{
		char Filename[128];
		sprintf(Filename, "/sys/class/power_supply/ADP%d/online", IdxBattery);

		int State = open(Filename, O_RDONLY);
		if (State != -1)
		{
			// found ACAD device. check its state.
			ssize_t ReadBytes = read(State, Scratch, 1);
			close(State);

			if (ReadBytes > 0)
			{
				bIsOnBattery = (Scratch[0] == '0');
			}

			break;	// quit checking after we found at least one
		}
	}

	// lack of ADP most likely means that we're not on laptop at all

	return bIsOnBattery;
}

#if !UE_BUILD_SHIPPING
CORE_API TFunction<void()> UngrabAllInputCallback;

void FLinuxPlatformMisc::DebugBreak()
{
	if( IsDebuggerPresent() )
	{
		if(UngrabAllInputCallback)
		{
			UngrabAllInputCallback();
		}
#if PLATFORM_CPU_X86_FAMILY
		__asm__ volatile("int $0x03");
#else
		raise(SIGTRAP);
#endif
	}
}
#endif // !UE_BUILD_SHIPPING
