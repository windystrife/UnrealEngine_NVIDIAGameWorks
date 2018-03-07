// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidPlatformStackWalk.h: Android platform stack walk functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformStackWalk.h"

/**
* Android platform stack walking
*/
struct CORE_API FAndroidPlatformStackWalk : public FGenericPlatformStackWalk
{
	typedef FGenericPlatformStackWalk Parent;

	static void ProgramCounterToSymbolInfo(uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo);
	static void CaptureStackBackTrace(uint64* BackTrace, uint32 MaxDepth, void* Context = nullptr);
};

typedef FAndroidPlatformStackWalk FPlatformStackWalk;
