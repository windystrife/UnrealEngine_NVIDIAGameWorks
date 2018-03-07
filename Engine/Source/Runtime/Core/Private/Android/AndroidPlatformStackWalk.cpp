// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidPlatformStackWalk.cpp: Android implementations of stack walk functions
=============================================================================*/

#include "AndroidPlatformStackWalk.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CString.h"
#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <stdio.h>

void FAndroidPlatformStackWalk::ProgramCounterToSymbolInfo(uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo)
{
	Dl_info DylibInfo;
	int32 Result = dladdr((const void*)ProgramCounter, &DylibInfo);
	if (Result == 0)
	{
		return;
	}

	int32 Status = 0;
	ANSICHAR* DemangledName = NULL;

	// Increased the size of the demangle destination to reduce the chances that abi::__cxa_demangle will allocate
	// this causes the app to hang as malloc isn't signal handler safe. Ideally we wouldn't call this function in a handler.
	size_t DemangledNameLen = 8192;
	ANSICHAR DemangledNameBuffer[8192] = { 0 };
	DemangledName = abi::__cxa_demangle(DylibInfo.dli_sname, DemangledNameBuffer, &DemangledNameLen, &Status);

	if (DemangledName)
	{
		// C++ function
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s ", DemangledName);
	}
	else if (DylibInfo.dli_sname)
	{
		// C function
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s() ", DylibInfo.dli_sname);
	}
	else
	{
		// Unknown!
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "[Unknown]() ");
	}

	// No line number available.
	// TODO open libUE4.so from the apk and get the DWARF-2 data.
	FCStringAnsi::Strcat(out_SymbolInfo.Filename, "Unknown");
	out_SymbolInfo.LineNumber = 0;
	
	// Offset of the symbol in the module, eg offset into libUE4.so needed for offline addr2line use.
	out_SymbolInfo.OffsetInModule = ProgramCounter - (uint64)DylibInfo.dli_fbase;

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

namespace AndroidStackWalkHelpers
{
	uint64* BackTrace;
	uint32 MaxDepth;

	static _Unwind_Reason_Code BacktraceCallback(struct _Unwind_Context* Context, void* InDepthPtr)
	{
		uint32* DepthPtr = (uint32*)InDepthPtr;

		if (*DepthPtr < MaxDepth)
		{
			BackTrace[*DepthPtr] = (uint64)_Unwind_GetIP(Context);
		}

		(*DepthPtr)++;
		return (_Unwind_Reason_Code)0;
	}
}

extern int32 unwind_backtrace_signal(void* sigcontext, uint64* Backtrace, int32 MaxDepth);

void FAndroidPlatformStackWalk::CaptureStackBackTrace(uint64* BackTrace, uint32 MaxDepth, void* Context)
{
	// Make sure we have place to store the information
	if (BackTrace == NULL || MaxDepth == 0)
	{
		return;
	}

	// zero results
	FPlatformMemory::Memzero(BackTrace, MaxDepth*sizeof(uint64));
	
#if PLATFORM_ANDROID_ARM
	if (Context != nullptr)
	{
		// Android signal handlers always catch signals before user handlers and passes it down to user later
		// _Unwind_Backtrace does not use signal context and will produce wrong callstack in this case
		// We use code from libcorkscrew to unwind backtrace using actual signal context
		// Code taken from https://android.googlesource.com/platform/system/core/+/jb-dev/libcorkscrew/arch-arm/backtrace-arm.c
		unwind_backtrace_signal(Context, BackTrace, MaxDepth);
		return;
	}
#endif //PLATFORM_ANDROID_ARM
	
	AndroidStackWalkHelpers::BackTrace = BackTrace;
	AndroidStackWalkHelpers::MaxDepth = MaxDepth;
	uint32 Depth = 0;
	_Unwind_Backtrace(AndroidStackWalkHelpers::BacktraceCallback, &Depth);
}
