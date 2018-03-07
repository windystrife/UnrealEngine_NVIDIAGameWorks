// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"

/**
 * Windows implementation of the stack walking.
 **/
struct CORE_API FWindowsPlatformStackWalk
	: public FGenericPlatformStackWalk
{
	static bool InitStackWalking();
	
	static TArray<FProgramCounterSymbolInfo> GetStack(int32 IgnoreCount, int32 MaxDepth = 100, void* Context = nullptr);

	static void ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo );
	static void CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context = nullptr );
	static void StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context = nullptr );
	static void ThreadStackWalkAndDump(ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, uint32 ThreadId);

	static int32 GetProcessModuleCount();
	static int32 GetProcessModuleSignatures(FStackWalkModuleInfo *ModuleSignatures, const int32 ModuleSignaturesSize);

	static void RegisterOnModulesChanged();

	/**
	 * Upload localy built symbols to network symbol storage.
	 *
	 * Use case:
	 *   Game designers use game from source (without prebuild game .dll-files).
	 *   In this case all game .dll-files are compiled locally.
	 *   For post-mortem debug programmers need .dll and .pdb files from designers.
	 */
	static bool UploadLocalSymbols();

	/**
	 * Get downstream storage with downloaded from remote symbol storage files.
	 */
	static FString GetDownstreamStorage();
};


typedef FWindowsPlatformStackWalk FPlatformStackWalk;
