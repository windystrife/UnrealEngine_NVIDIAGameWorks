// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5StackWalk.h: HTML5 platform stack walk functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformStackWalk.h"

/**
* HTML5 platform stack walking
*/
struct CORE_API FHTML5PlatformStackWalk : public FGenericPlatformStackWalk
{
	typedef FGenericPlatformStackWalk Parent;

	static void ProgramCounterToSymbolInfo(uint64 ProgramCounter,FProgramCounterSymbolInfo& out_SymbolInfo);
	static void CaptureStackBackTrace(uint64* BackTrace,uint32 MaxDepth,void* Context = nullptr);
	//Additional
	static int32 GetStackBackTraceString(char* OutputString, int32 MaxLen);
};

typedef FHTML5PlatformStackWalk FPlatformStackWalk;
