// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformStackWalk.mm: Apple implementations of stack walk functions
=============================================================================*/

#include "ApplePlatformStackWalk.h"
#include "ApplePlatformSymbolication.h"
#include "Containers/StringConv.h"
#include <execinfo.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <cxxabi.h>
#include "CoreGlobals.h"

#if PLATFORM_MAC
#include "PLCrashReporter.h"
#endif

// Internal helper functions not exposed to public
int32 GetModuleImageSize( const struct mach_header* Header )
{
#if PLATFORM_64BITS
	struct load_command *CurrentCommand = (struct load_command *)( (char *)Header + sizeof(struct mach_header_64) );
	int32 ModuleSize = 0;
	
	// Check header
	if( Header->magic != MH_MAGIC_64 )
		return 0;
#else
	struct load_command *CurrentCommand = (struct load_command *)( (char *)Header + sizeof(struct mach_header) );
	int32 ModuleSize = 0;
	
	// Check header
	if( Header->magic != MH_MAGIC )
		return 0;
#endif
	
	for( int32 i = 0; i < Header->ncmds; i++ )
	{
		if( CurrentCommand->cmd == LC_SEGMENT )
		{
			struct segment_command *SegmentCommand = (struct segment_command *) CurrentCommand;
			ModuleSize += SegmentCommand->vmsize;
		}
		
		CurrentCommand = (struct load_command *)( (char *)CurrentCommand + CurrentCommand->cmdsize );
	}
	
	return ModuleSize;
}

int32 GetModuleTimeStamp( const struct mach_header* Header )
{
#if PLATFORM_64BITS
	struct load_command *CurrentCommand = (struct load_command *)( (char *)Header + sizeof(struct mach_header_64) );
	int32 ModuleSize = 0;
	
	// Check header
	if( Header->magic != MH_MAGIC_64 )
		return 0;
#else
	struct load_command *CurrentCommand = (struct load_command *)( (char *)Header + sizeof(struct mach_header) );
	int32 ModuleSize = 0;
	
	// Check header
	if( Header->magic != MH_MAGIC )
		return 0;
#endif
	
	for( int32 i = 0; i < Header->ncmds; i++ )
	{
		if( CurrentCommand->cmd == LC_LOAD_DYLIB ) //LC_ID_DYLIB )
		{
			struct dylib_command *DylibCommand = (struct dylib_command *) CurrentCommand;
			return DylibCommand->dylib.timestamp;
		}
		
		CurrentCommand = (struct load_command *)( (char *)CurrentCommand + CurrentCommand->cmdsize );
	}
	
	return 0;
}

static void AsyncSafeProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
{
	Dl_info DylibInfo;
	int32 Result = dladdr((const void*)ProgramCounter, &DylibInfo);
	if (Result == 0)
	{
		return;
	}

#if PLATFORM_MAC && IS_PROGRAM // On the Mac the crash report client can resymbolise
	if (DylibInfo.dli_sname)
	{
		// Mangled function name
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s ", DylibInfo.dli_sname);
	}
	else
	{
		// Unknown!
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "[Unknown]() ");
	}
#else // But on iOS the best we can do is demangle
	int32 Status = 0;
	ANSICHAR* DemangledName = NULL;
	
	// Increased the size of the demangle destination to reduce the chances that abi::__cxa_demangle will allocate
	// this causes the app to hang as malloc isn't signal handler safe. Ideally we wouldn't call this function in a handler.
	size_t DemangledNameLen = 65536;
	ANSICHAR DemangledNameBuffer[65536]= {0};
	DemangledName = abi::__cxa_demangle(DylibInfo.dli_sname, DemangledNameBuffer, &DemangledNameLen, &Status);
	
	if (DemangledName)
	{
		// C++ function
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s ", DemangledName);
	}
	else if (DylibInfo.dli_sname && strchr(DylibInfo.dli_sname, ']'))
	{
		// ObjC function
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s ", DylibInfo.dli_sname);
	}
	else if(DylibInfo.dli_sname)
	{
		// C function
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s() ", DylibInfo.dli_sname);
	}
	else
	{
		// Unknown!
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "[Unknown]() ");
	}
#endif
	
	// No line number found.
	FCStringAnsi::Strcat(out_SymbolInfo.Filename, "Unknown");
	out_SymbolInfo.LineNumber = 0;
	
	// Write out Module information.
	ANSICHAR* DylibPath = (ANSICHAR*)DylibInfo.dli_fname;
	ANSICHAR* DylibName = FCStringAnsi::Strrchr(DylibPath, '/');
	if (DylibName)
	{
		DylibName += 1;
	}
	else
	{
		DylibName = DylibPath;
	}
	FCStringAnsi::Strcpy(out_SymbolInfo.ModuleName, DylibName);
}

void FApplePlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{
	// Make sure we have place to store the information before we go through the process of raising
	// an exception and handling it.
	if( BackTrace == NULL || MaxDepth == 0 )
	{
		return;
	}

#if PLATFORM_MAC
	if(Context)
	{
		int i = plcrashreporter_backtrace((void**)BackTrace, MaxDepth);
		
		return;
	}
#endif
	backtrace((void**)BackTrace, MaxDepth);
}

bool FApplePlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
{
	
	//
	// Callstack lines should be written in this standard format
	//
	//	0xaddress module!func [file]
	// 
	// E.g. 0x045C8D01 OrionClient.self!UEngine::PerformError() [D:\Epic\Orion\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:6481]
	//
	// Module may be omitted, everything else should be present, or substituted with a string that conforms to the expected type
	//
	// E.g 0x00000000 UnknownFunction []
	//
	// 

	Dl_info DylibInfo;
	int32 Result = dladdr((const void*)ProgramCounter, &DylibInfo);
	if (Result == 0)
	{
		return false;
	}

	FProgramCounterSymbolInfo SymbolInfo;
	if(!Context)
	{
		ProgramCounterToSymbolInfo( ProgramCounter, SymbolInfo );
	}
	else
	{
		AsyncSafeProgramCounterToSymbolInfo(ProgramCounter, SymbolInfo);
	}

	ANSICHAR TempArray[MAX_SPRINTF];

	// Write out prefix, address, module, and function na,e
	FCStringAnsi::Sprintf(TempArray, "0x%08x %s!%s ", (uint32)ProgramCounter, SymbolInfo.ModuleName, SymbolInfo.FunctionName);
	
	FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, TempArray);

	// Get filename.
	{
		ANSICHAR FileNameLine[MAX_SPRINTF];
		
		if(SymbolInfo.LineNumber == 0)
		{
			// No line number. Print out the logical address instead.
			FCStringAnsi::Sprintf(FileNameLine, " [UnknownFile]) ", ProgramCounter);
		}
		else
		{
			// try to add source file and line number, too
			FCStringAnsi::Sprintf(FileNameLine, " [%s:%d] ", SymbolInfo.Filename, SymbolInfo.LineNumber);
		}
		
		FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, FileNameLine);
	}

	// For the crash reporting code this needs a Windows line ending, the caller is responsible for the '\n'
	FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, "\r");

	return true;
}

void FApplePlatformStackWalk::ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
{
	bool bOK = FApplePlatformSymbolication::SymbolInfoForAddress(ProgramCounter, out_SymbolInfo);
	if(!bOK)
	{
		AsyncSafeProgramCounterToSymbolInfo(ProgramCounter, out_SymbolInfo);
	}
}

int32 FApplePlatformStackWalk::GetProcessModuleCount()
{
	return (int32)_dyld_image_count();
}

int32 FApplePlatformStackWalk::GetProcessModuleSignatures(FStackWalkModuleInfo *ModuleSignatures, const int32 ModuleSignaturesSize)
{
	int32 ModuleCount = GetProcessModuleCount();

	int32 SignatureIndex = 0;

	for (int32 ModuleIndex = 0; ModuleIndex < ModuleCount && SignatureIndex < ModuleSignaturesSize; ModuleIndex++)
	{
		const struct mach_header* Header = _dyld_get_image_header(ModuleIndex);
		const ANSICHAR* ImageName = _dyld_get_image_name(ModuleIndex);
		if (Header && ImageName)
		{
			FStackWalkModuleInfo Info;
			Info.BaseOfImage = (uint64)Header;
			FCString::Strcpy(Info.ImageName, ANSI_TO_TCHAR(ImageName));
			Info.ImageSize = GetModuleImageSize( Header );
			FCString::Strcpy(Info.LoadedImageName, ANSI_TO_TCHAR(ImageName));
			FCString::Strcpy(Info.ModuleName, ANSI_TO_TCHAR(ImageName));
			Info.PdbAge = 0;
			Info.PdbSig = 0;
			FMemory::Memzero(&Info.PdbSig70, sizeof(Info.PdbSig70));
			Info.TimeDateStamp = GetModuleTimeStamp( Header );

			ModuleSignatures[SignatureIndex] = Info;
			++SignatureIndex;
		}
	}
	
	return SignatureIndex;
}

void CreateExceptionInfoString(int32 Signal, struct __siginfo* Info)
{
	FString ErrorString = TEXT("Unhandled Exception: ");
	
#define HANDLE_CASE(a,b) case a: ErrorString += TEXT(#a #b); break;
	
	switch (Signal)
	{
		case SIGSEGV:
			ErrorString += TEXT("SIGSEGV segmentation violation at address ");
			ErrorString += FString::Printf(TEXT("0x%08x"), (uint32*)Info->si_addr);
			break;
		case SIGBUS:
			ErrorString += TEXT("SIGBUS bus error at address ");
			ErrorString += FString::Printf(TEXT("0x%08x"), (uint32*)Info->si_addr);
			break;
			
		HANDLE_CASE(SIGINT, "interrupt program")
		HANDLE_CASE(SIGQUIT, "quit program")
		HANDLE_CASE(SIGILL, "illegal instruction")
		HANDLE_CASE(SIGTRAP, "trace trap")
		HANDLE_CASE(SIGABRT, "abort() call")
		HANDLE_CASE(SIGFPE, "floating-point exception")
		HANDLE_CASE(SIGKILL, "kill program")
		HANDLE_CASE(SIGSYS, "non-existent system call invoked")
		HANDLE_CASE(SIGPIPE, "write on a pipe with no reader")
		HANDLE_CASE(SIGTERM, "software termination signal")
		HANDLE_CASE(SIGSTOP, "stop")

		default:
			ErrorString += FString::Printf(TEXT("0x%08x"), (uint32)Signal);
	}
	
#if WITH_EDITORONLY_DATA
	FCString::Strncpy(GErrorExceptionDescription, *ErrorString, FMath::Min(ErrorString.Len() + 1, (int32)ARRAY_COUNT(GErrorExceptionDescription)));
#endif
#undef HANDLE_CASE
}

int32 ReportCrash(ucontext_t *Context, int32 Signal, struct __siginfo* Info)
{
	static bool GAlreadyCreatedMinidump = false;
	
	// Only create a minidump the first time this function is called.
	// (Can be called the first time from the RenderThread, then a second time from the MainThread.)
	if ( GAlreadyCreatedMinidump == false )
	{
		GAlreadyCreatedMinidump = true;

		// No malloc in a signal handler as it is unsafe & will deadlock the application!
		const SIZE_T StackTraceSize = 65535;
		static ANSICHAR StackTrace[ StackTraceSize ];
		StackTrace[0] = 0;
		
		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump( StackTrace, StackTraceSize, 0, Context );
#if WITH_EDITORONLY_DATA
        FCString::Strncat( GErrorHist, ANSI_TO_TCHAR(StackTrace), ARRAY_COUNT(GErrorHist) - 1 );
		CreateExceptionInfoString(Signal, Info);
#endif
	}

	return 0;
}
