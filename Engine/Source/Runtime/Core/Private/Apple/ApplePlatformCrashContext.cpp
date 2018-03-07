// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformCrashContext.cpp: Common implementations of Apple platform crash context.
=============================================================================*/

#include "Apple/ApplePlatformCrashContext.h"
#include "Apple/ApplePlatformSymbolication.h"
#include "Containers/StringConv.h"
#include "CoreGlobals.h"

FApplePlatformCrashContext::FApplePlatformCrashContext()
:	Signal(0)
,	Info(NULL)
,	Context(NULL)
,	IgnoreDepth(6)
{
	SignalDescription[ 0 ] = 0;
	MinidumpCallstackInfo[ 0 ] = 0;
}

FApplePlatformCrashContext::~FApplePlatformCrashContext()
{
	
}

void FApplePlatformCrashContext::InitFromSignal(int32 InSignal, siginfo_t* InInfo, void* InContext)
{
	Signal = InSignal;
	Info = InInfo;
	Context = reinterpret_cast< ucontext_t* >( InContext );
	
#define HANDLE_CASE(a,b) case a: FCStringAnsi::Strncpy(SignalDescription, #a ": " b, ARRAY_COUNT( SignalDescription ) - 1); break;
	switch (Signal)
	{
		case SIGSEGV:
			FCStringAnsi::Strncpy(SignalDescription, "SIGSEGV: invalid attempt to access memory at address 0x", ARRAY_COUNT( SignalDescription ) - 1);
			FCStringAnsi::Strcat(SignalDescription, ItoANSI((uintptr_t)Info->si_addr, 16));
			break;
		case SIGBUS:
			FCStringAnsi::Strncpy(SignalDescription, "SIGBUS: invalid attempt to access memory at address 0x", ARRAY_COUNT( SignalDescription ) - 1);
			FCStringAnsi::Strcat(SignalDescription, ItoANSI((uintptr_t)Info->si_addr, 16));
			break;
			HANDLE_CASE(SIGINT, "program interrupted")
			HANDLE_CASE(SIGQUIT, "user-requested crash")
			HANDLE_CASE(SIGILL, "illegal instruction")
			HANDLE_CASE(SIGTRAP, "trace trap")
			HANDLE_CASE(SIGABRT, "abort() called")
			HANDLE_CASE(SIGFPE, "floating-point exception")
			HANDLE_CASE(SIGKILL, "program killed")
			HANDLE_CASE(SIGSYS, "non-existent system call invoked")
			HANDLE_CASE(SIGPIPE, "write on a pipe with no reader")
			HANDLE_CASE(SIGTERM, "software termination signal")
			HANDLE_CASE(SIGSTOP, "stop")
			
		default:
			FCStringAnsi::Strncpy(SignalDescription, "Signal ", ARRAY_COUNT( SignalDescription ) - 1);
			FCStringAnsi::Strcat(SignalDescription, ItoANSI(Signal, 10));
			FCStringAnsi::Strcat(SignalDescription, " (unknown)");
			break;
	}
#undef HANDLE_CASE
}

int32 FApplePlatformCrashContext::ReportCrash() const
{
	static bool GAlreadyCreatedMinidump = false;
	// Only create a minidump the first time this function is called.
	// (Can be called the first time from the RenderThread, then a second time from the MainThread.)
	if ( GAlreadyCreatedMinidump == false )
	{
		GAlreadyCreatedMinidump = true;
		
		ANSICHAR* StackBuffer = const_cast<ANSICHAR*>(&MinidumpCallstackInfo[0]);
		*StackBuffer = 0;
		
		// Walk the stack and dump it to the allocated memory (ignore first 2 callstack lines as those are in stack walking code)
		FPlatformStackWalk::StackWalkAndDump( StackBuffer, ARRAY_COUNT(MinidumpCallstackInfo) - 1, IgnoreDepth, Context );
		
		FUTF8ToTCHAR_Convert::Convert(GErrorHist, ARRAY_COUNT(GErrorHist) - 1, MinidumpCallstackInfo, FCStringAnsi::Strlen(MinidumpCallstackInfo));
		CreateExceptionInfoString(Signal, Info);
	}
	
	return 0;
}

void FApplePlatformCrashContext::CreateExceptionInfoString(int32 Signal, siginfo_t* Info)
{
#define HANDLE_CASE(a,b) case a: FCString::Strncpy(GErrorExceptionDescription, TEXT(#a ": " b), ARRAY_COUNT( SignalDescription ) - 1); break;
	switch (Signal)
	{
		case SIGSEGV:
			FCString::Strncpy(GErrorExceptionDescription, TEXT("SIGSEGV: invalid attempt to access memory at address 0x"), ARRAY_COUNT( SignalDescription ) - 1);
			FCString::Strcat(GErrorExceptionDescription, ItoTCHAR((uintptr_t)Info->si_addr, 16));
			break;
		case SIGBUS:
			FCString::Strncpy(GErrorExceptionDescription, TEXT("SIGBUS: invalid attempt to access memory at address 0x"), ARRAY_COUNT( SignalDescription ) - 1);
			FCString::Strcat(GErrorExceptionDescription, ItoTCHAR((uintptr_t)Info->si_addr, 16));
			break;
			HANDLE_CASE(SIGINT, "program interrupted")
			HANDLE_CASE(SIGQUIT, "user-requested crash")
			HANDLE_CASE(SIGILL, "illegal instruction")
			HANDLE_CASE(SIGTRAP, "trace trap")
			HANDLE_CASE(SIGABRT, "abort() called")
			HANDLE_CASE(SIGFPE, "floating-point exception")
			HANDLE_CASE(SIGKILL, "program killed")
			HANDLE_CASE(SIGSYS, "non-existent system call invoked")
			HANDLE_CASE(SIGPIPE, "write on a pipe with no reader")
			HANDLE_CASE(SIGTERM, "software termination signal")
			HANDLE_CASE(SIGSTOP, "stop")
			
		default:
			FCString::Strncpy(GErrorExceptionDescription, TEXT("Signal "), ARRAY_COUNT( SignalDescription ) - 1);
			FCString::Strcat(GErrorExceptionDescription, ItoTCHAR(Signal, 10));
			FCString::Strcat(GErrorExceptionDescription, TEXT(" (unknown)"));
			break;
	}
#undef HANDLE_CASE
}

void FApplePlatformCrashContext::WriteLine(int ReportFile, const ANSICHAR* Line)
{
	if( Line != NULL )
	{
		int64 StringBytes = FCStringAnsi::Strlen(Line);
		write(ReportFile, Line, StringBytes);
	}
	
	// use Windows line terminator
	static ANSICHAR WindowsTerminator[] = "\r\n";
	write(ReportFile, WindowsTerminator, 2);
}

void FApplePlatformCrashContext::WriteUTF16String(int ReportFile, const TCHAR* UTFString4BytesChar, uint32 NumChars)
{
	check(UTFString4BytesChar != NULL || NumChars == 0);
	static_assert(sizeof(TCHAR) == 4, "Platform TCHAR is not 4 bytes. Revisit this function.");
	
	for (uint32 Idx = 0; Idx < NumChars; ++Idx)
	{
		write(ReportFile, &UTFString4BytesChar[Idx], 2);
	}
}

void FApplePlatformCrashContext::WriteUTF16String(int ReportFile, const TCHAR* UTFString4BytesChar)
{
	static_assert(sizeof(TCHAR) == 4, "Platform TCHAR is not 4 bytes. Revisit this function.");
	uint32 NumChars = FCString::Strlen(UTFString4BytesChar);
	WriteUTF16String(ReportFile, UTFString4BytesChar, NumChars);
}

void FApplePlatformCrashContext::WriteLine(int ReportFile, const TCHAR* Line)
{
	if( Line != NULL )
	{
		int64 NumChars = FCString::Strlen(Line);
		WriteUTF16String(ReportFile, Line, NumChars);
	}
	
	// use Windows line terminator
	static TCHAR WindowsTerminator[] = TEXT("\r\n");
	WriteUTF16String(ReportFile, WindowsTerminator, 2);
}

ANSICHAR* FApplePlatformCrashContext::ItoANSI(uint64 Val, uint64 Base, uint32 Len)
{
	static ANSICHAR InternalBuffer[64] = {0};
	
	uint64 i = 62;
	int32 pad = Len;
	
	if(Val)
	{
		for(; Val && i ; --i, Val /= Base, --pad)
		{
			InternalBuffer[i] = "0123456789abcdef"[Val % Base];
		}
	}
	else
	{
		InternalBuffer[i--] = '0';
		--pad;
	}
	
	while(pad > 0)
	{
		InternalBuffer[i--] = '0';
		--pad;
	}
	
	return &InternalBuffer[i+1];
}

TCHAR* FApplePlatformCrashContext::ItoTCHAR(uint64 Val, uint64 Base, uint32 Len)
{
	static TCHAR InternalBuffer[64] = {0};
	
	uint64 i = 62;
	int32 pad = Len;
	
	if(Val)
	{
		for(; Val && i ; --i, Val /= Base, --pad)
		{
			InternalBuffer[i] = TEXT("0123456789abcdef")[Val % Base];
		}
	}
	else
	{
		InternalBuffer[i--] = TEXT('0');
		--pad;
	}
	
	while(pad > 0)
	{
		InternalBuffer[i--] = '0';
		--pad;
	}
	
	return &InternalBuffer[i+1];
}
