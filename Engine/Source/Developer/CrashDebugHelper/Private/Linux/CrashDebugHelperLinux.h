// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CrashDebugHelper.h"

class FString;

class FCrashDebugHelperLinux : public ICrashDebugHelper
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
};

typedef FCrashDebugHelperLinux FCrashDebugHelper;
