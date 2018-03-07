// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformStackWalk.h: Apple platform stack walk functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformStackWalk.h"

/**
 * Apple platform implementation of the misc OS functions
 */
struct CORE_API FApplePlatformStackWalk : public FGenericPlatformStackWalk
{
	static bool ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context = nullptr );
	static void ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo&  out_SymbolInfo);
	static void CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context = nullptr );
	static int32 GetProcessModuleCount();
	static int32 GetProcessModuleSignatures(FStackWalkModuleInfo *ModuleSignatures, const int32 ModuleSignaturesSize);
};

typedef FApplePlatformStackWalk FPlatformStackWalk;
