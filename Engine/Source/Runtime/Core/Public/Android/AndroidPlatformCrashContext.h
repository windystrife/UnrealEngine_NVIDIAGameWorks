// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformCrashContext.h"

struct CORE_API FAndroidCrashContext : public FGenericCrashContext
{
	/** Signal number */
	int32 Signal;
	
	/** Additional signal info */
	siginfo* Info;
	
	/** Thread context */
	void* Context;

	FAndroidCrashContext()
		:	Signal(0)
		,	Info(NULL)
		,	Context(NULL)
	{
	}

	~FAndroidCrashContext()
	{
	}

	/**
	 * Inits the crash context from data provided by a signal handler.
	 *
	 * @param InSignal number (SIGSEGV, etc)
	 * @param InInfo additional info (e.g. address we tried to read, etc)
	 * @param InContext thread context
	 */
	void InitFromSignal(int32 InSignal, siginfo* InInfo, void* InContext)
	{
		Signal = InSignal;
		Info = InInfo;
		Context = InContext;
	}
};

typedef FAndroidCrashContext FPlatformCrashContext;