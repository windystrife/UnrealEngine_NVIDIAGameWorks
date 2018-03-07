// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashDebugHelper.h"

class FCrashDebugHelperWindows : public ICrashDebugHelper
{
public:
	/**
	 *	Parse the given crash dump, and generate a report. 
	 *
	 *	@param	InCrashDumpName		The crash dump file to process
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool CreateMinidumpDiagnosticReport( const FString& InCrashDumpName ) override;

private:
	bool InitSymbols(struct FWindowsPlatformStackWalkExt& WindowsStackWalkExt, bool bSyncSymbols);
	void SyncAndReadSourceFile(bool bSyncSymbols, bool bAnnotate, int32 BuiltFromCL);
};

typedef FCrashDebugHelperWindows FCrashDebugHelper;
