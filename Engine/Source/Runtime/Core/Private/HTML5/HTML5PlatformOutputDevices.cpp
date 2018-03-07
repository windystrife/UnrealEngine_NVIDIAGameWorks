// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5PlatformOutputDevices.h"
#include "HAL/OutputDevices.h"
#include <emscripten/trace.h>

class FTraceOutputDevice : public FOutputDevice
{
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
	{
		emscripten_trace_log_message(Category.GetPlainANSIString(), TCHAR_TO_ANSI(V));
	}
};

FOutputDevice* FHTML5PlatformOutputDevices::GetLog()
{
	static FTraceOutputDevice Singleton;
	return &Singleton;
}
